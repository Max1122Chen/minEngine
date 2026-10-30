#include "Runtime/Core/Assert/AssertLog.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <string>

namespace minEngine
{
    namespace
    {
        /** Records the staging area holds before further failures are only counted (never dropped silently). */
        constexpr std::uint32_t kContractRecordSlots = 64;

        /**
         * A staged record. Plain integers and bytes, written with the interlocked helpers in
         * Assert.h: the reporter may not construct anything, so a slot is claimed by a compare and
         * exchange rather than by a lock.
         */
        struct ContractRecordSlot
        {
            std::uint32_t State; // 0 free, 1 claimed, 2 staged and waiting for the log
            std::uint32_t Reserved;
            char Text[kContractRecordTextCapacity];
        };

        /** Counters and staging area: one instance for the whole process. */
        struct ContractState
        {
            std::uint64_t Ensured;
            std::uint64_t Trapped;
            std::uint64_t Suppressed;
            std::uint64_t Undelivered;
            std::uint32_t NextSlot;
            std::uint32_t Reserved;
            // Shared on purpose: a report raised in one module must still name the role its thread
            // holds, and only the module that owns the frame-role table can answer that. A function
            // pointer is a process-wide address, so every module can call it.
            ContractRoleResolver RoleResolver;
            ContractRecordSlot Slots[kContractRecordSlots];
        };

        /**
         * The one block. It has no constructor and no member initialisers, so it lives in the
         * module's zero-initialised data: reaching it takes no guard variable, no heap and no lock
         * on the first report, and it is the same object before and after every other module has
         * initialised.
         *
         * One block for the process is a property of where this sits, not of a lookup: the
         * definition is here, in the engine module, and every other module reaches it through the
         * exported functions below. There is no second definition to instantiate, so a module cannot
         * end up counting into a copy of its own.
         */
        ContractState g_ContractState;

        /**
         * Claims each staged record for this drain and hands it to the log. Two threads draining at
         * the same time publish one record once, and a record staged by a thread that is still
         * holding a lock is published here by a thread that is not.
         */
        std::uint32_t PublishStagedRecords()
        {
            std::uint32_t published = 0;

            for (std::uint32_t index = 0; index < kContractRecordSlots; ++index)
            {
                ContractRecordSlot& slot = g_ContractState.Slots[index];
                if (ContractDetail::LoadAcquire32(&slot.State) != 2u)
                {
                    continue;
                }

                if (ContractDetail::CompareExchange32(&slot.State, 1u, 2u) != 2u)
                {
                    continue;
                }

                const std::string text(slot.Text);
                ContractDetail::StoreRelease32(&slot.State, 0u);

                // The failing thread is named inside the text: Emit stamps the publishing thread,
                // and a reader needs the thread that broke the contract, not the one that logged it.
                LogSystem::Emit(LogCore, LogSeverity::Error, text, LogSourceLocation{nullptr, 0});
                ++published;
            }

            return published;
        }
    }

    MINENGINE_API void CountContractViolation(ContractGrade grade, bool bSuppressed)
    {
        if (grade == ContractGrade::AssertFatal)
        {
            ContractDetail::Increment64(&g_ContractState.Trapped);
        }
        else
        {
            ContractDetail::Increment64(&g_ContractState.Ensured);
        }
        if (bSuppressed)
        {
            ContractDetail::Increment64(&g_ContractState.Suppressed);
        }
    }

    MINENGINE_API void StageContractRecord(const char* text)
    {
        if (text == nullptr)
        {
            return;
        }

        for (std::uint32_t attempt = 0; attempt < kContractRecordSlots; ++attempt)
        {
            const std::uint32_t index =
                ContractDetail::Increment32(&g_ContractState.NextSlot) % kContractRecordSlots;
            ContractRecordSlot& slot = g_ContractState.Slots[index];
            if (ContractDetail::CompareExchange32(&slot.State, 1u, 0u) != 0u)
            {
                continue;
            }

            std::size_t copied = 0;
            for (; text[copied] != '\0' && copied + 1 < kContractRecordTextCapacity; ++copied)
            {
                slot.Text[copied] = text[copied];
            }
            slot.Text[copied] = '\0';
            ContractDetail::StoreRelease32(&slot.State, 2u);
            return;
        }
        ContractDetail::Increment64(&g_ContractState.Undelivered);
    }

    MINENGINE_API void SetContractRoleResolver(ContractRoleResolver resolver)
    {
        g_ContractState.RoleResolver = resolver;
    }

    MINENGINE_API const char* CurrentContractRole()
    {
        const ContractRoleResolver resolver = g_ContractState.RoleResolver;
        if (resolver == nullptr)
        {
            return "unbound (no frame-role resolver installed)";
        }
        const char* role = resolver();
        return role != nullptr ? role : "unbound";
    }

    MINENGINE_API ContractViolationCounters GetContractViolationCounters()
    {
        ContractViolationCounters counters;
        counters.RecordedOnContinue = ContractDetail::LoadRelaxed64(&g_ContractState.Ensured);
        counters.RecordedOnTrap = ContractDetail::LoadRelaxed64(&g_ContractState.Trapped);
        counters.Suppressed = ContractDetail::LoadRelaxed64(&g_ContractState.Suppressed);
        counters.Undelivered = ContractDetail::LoadRelaxed64(&g_ContractState.Undelivered);
        return counters;
    }

    MINENGINE_API ContractViolationFrameSample PublishAndSampleContractViolations()
    {
        ContractViolationFrameSample sample;
        sample.Published = PublishStagedRecords();
        sample.Counters = GetContractViolationCounters();
        return sample;
    }
}
