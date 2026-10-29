#pragma once

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <string>

namespace minEngine
{
    /**
     * Publishes every contract violation staged since the last call as one Error record, and returns
     * how many were published.
     *
     * Call this from a thread that owns no log lock: the frame boundary is the intended one. The
     * report path cannot do it itself - LogSystem::Emit calls every sink while holding the log
     * mutex, so a violation raised from inside a sink is already holding it, and a report that asked
     * for it again would hang the process instead of recording the violation.
     */
    inline std::uint32_t DrainContractViolations()
    {
        ContractState& state = ContractDetail::State();
        std::uint32_t published = 0;

        for (std::uint32_t index = 0; index < kContractRecordSlots; ++index)
        {
            ContractRecordSlot& slot = state.Slots[index];
            if (ContractDetail::LoadAcquire32(&slot.State) != 2u)
            {
                continue;
            }

            // Claim the slot for this drain: two threads draining at the same time must publish one
            // record once, not twice.
            if (ContractDetail::CompareExchange32(&slot.State, 1u, 2u) != 2u)
            {
                continue;
            }

            const std::string text(slot.Text);
            ContractDetail::StoreRelease32(&slot.State, 0u);

            // The failing thread is named inside the text: Emit stamps the publishing thread, and a
            // reader needs the thread that broke the contract, not the one that logged it.
            LogSystem::Emit(LogCore, LogSeverity::Error, text, LogSourceLocation{nullptr, 0});
            ++published;
        }

        return published;
    }
}
