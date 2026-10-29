#pragma once

/**
 * Contract checks, in two grades.
 *
 * ME_ENSURE(condition, format, ...)
 *     A check the caller can survive: the failure is counted, recorded with file:line, thread and
 *     frame role, and execution continues. Always enabled, in every build.
 *
 * ME_ASSERT(condition, format, ...)
 *     A check whose continuation would touch memory or a resource whose precondition no longer
 *     holds. The same record, then a trap that stops the process, so the violated premise cannot
 *     turn into silent corruption. A shipped build cannot afford a dead build from one violated
 *     contract, so ME_ASSERT degrades to ME_ENSURE there: NDEBUG builds, and any build that sets
 *     ME_CONTRACT_ASSERT_FATAL=0 or ME_ASSERT_ENABLED=0.
 *
 * What a report may do, and why it is that small
 *     A contract violation often fires while the violating thread holds a lock. A report path that
 *     asks the log system for its mutex turns "a violation was logged" into "the process hung":
 *     LogSystem::Emit calls every sink while holding that mutex, so a violation raised from inside
 *     a sink is already holding it. A report therefore does exactly this:
 *       - one interlocked increment per counter;
 *       - one fixed-size record written into a preallocated, process-wide staging area;
 *       - one line written straight to the standard error handle.
 *     No allocation, no formatting through the C++ library, no log-system lock, no lock at all
 *     beyond the kernel's own object table on the very first report. Publishing the staged records
 *     to the log happens later, in DrainContractViolations() (AssertLog.h), from a caller that owns
 *     no log lock - the frame boundary owns one such moment.
 *
 * Rate limiting
 *     A violated contract is usually violated every frame, so a site that logged once per failure
 *     would bury the log it is trying to inform. Each site keeps a small budget table keyed on
 *     (message, role): the first kContractVerboseReportsPerViolation failures of a distinct
 *     violation are recorded in full, the next one states that the site has gone quiet, and any
 *     failure after that only moves the counters. Nothing is lost silently: the cumulative total
 *     keeps rising and the per-frame summary reports it.
 */

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>

#if defined(_WIN32)
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#ifndef ME_ASSERT_ENABLED
    #define ME_ASSERT_ENABLED 1
#endif

// The grade of ME_ASSERT is a build decision, not a call-site one: a release build keeps the record
// and drops the trap.
#ifndef ME_CONTRACT_ASSERT_FATAL
    #if defined(NDEBUG) || !ME_ASSERT_ENABLED
        #define ME_CONTRACT_ASSERT_FATAL 0
    #else
        #define ME_CONTRACT_ASSERT_FATAL 1
    #endif
#endif

#if defined(_WIN32)
// Two kernel32 entry points, declared here instead of through <windows.h>: this header is reached by
// every engine translation unit, and windows.h brings macros that rename engine functions (its
// GetClassName would rewrite the reflection template of the same name). Both signatures are fixed
// ABI, and both are declared in a namespace so a translation unit that includes windows.h as well
// keeps its own global declarations untouched.
namespace minEngine
{
    namespace ContractWin32
    {
        extern "C" __declspec(dllimport) void* __stdcall CreateFileMappingW(
            void* fileHandle,
            void* attributes,
            unsigned long protect,
            unsigned long maximumSizeHigh,
            unsigned long maximumSizeLow,
            const wchar_t* name);

        extern "C" __declspec(dllimport) void* __stdcall MapViewOfFile(
            void* mapping,
            unsigned long access,
            unsigned long offsetHigh,
            unsigned long offsetLow,
            std::size_t bytesToMap);
    }
}
#endif

namespace minEngine
{
    /** Text capacity of one staged record: a file:line, a thread, a role and the author's message. */
    constexpr std::uint32_t kContractRecordTextCapacity = 256;

    /** Records the staging area holds before further failures are only counted (never dropped silently). */
    constexpr std::uint32_t kContractRecordSlots = 64;

    /** Distinct violations a single site reports in full before it goes quiet. */
    constexpr std::uint32_t kContractVerboseReportsPerViolation = 4;

    /** Distinct (message, role) pairs a single site tracks separately. */
    constexpr std::uint32_t kContractBudgetedViolations = 8;

    using ContractRoleResolver = const char* (*)();

    /**
     * Which grade produced a report. A degraded ME_ASSERT keeps its own name in the record: a reader
     * who sees ME_ASSERT knows the contract was meant to be fatal and this build only recorded it.
     */
    enum class ContractGrade : std::uint32_t
    {
        Ensure = 0,
        AssertFatal = 1,
        AssertDegraded = 2,
    };

    /**
     * One report site: one expansion of ME_ENSURE or ME_ASSERT. Constant-initialised, so declaring
     * it costs no thread-safe static guard on the failure path.
     */
    struct ContractSite
    {
        const char* File = nullptr;
        int Line = 0;
        std::uint32_t BudgetKeys[kContractBudgetedViolations]{};
        std::uint32_t BudgetUsed[kContractBudgetedViolations]{};
    };

    /** A staged record. Plain integers and bytes: it lives in memory that is mapped, not constructed. */
    struct ContractRecordSlot
    {
        std::uint32_t State = 0; // 0 free, 1 claimed, 2 staged and waiting for the log
        std::uint32_t Reserved = 0;
        char Text[kContractRecordTextCapacity]{};
    };

    /** Counters and staging area: one instance per process, shared by every module that reports. */
    struct ContractState
    {
        std::uint64_t Ensured = 0;
        std::uint64_t Trapped = 0;
        std::uint64_t Suppressed = 0;
        std::uint64_t Undelivered = 0;
        std::uint32_t NextSlot = 0;
        std::uint32_t Reserved = 0;
        // Shared on purpose: a report raised in one module must still name the role its thread holds,
        // and only the module that owns the frame-role table can answer that. A function pointer is
        // a process-wide address, so every module can call it.
        ContractRoleResolver RoleResolver = nullptr;
        ContractRecordSlot Slots[kContractRecordSlots]{};
    };

    /** What the report path has counted so far. */
    struct ContractViolationCounters
    {
        /** ME_ENSURE failures: recorded and continued. */
        std::uint64_t RecordedOnContinue = 0;
        /** ME_ASSERT failures under the fatal policy: recorded, then the process stopped. */
        std::uint64_t RecordedOnTrap = 0;
        /** Failures kept out of the log by a site's report budget. Counted, not lost. */
        std::uint64_t Suppressed = 0;
        /** Failures whose record found no free slot in the staging area. */
        std::uint64_t Undelivered = 0;
        /** True when the counters are shared with every module in this process. */
        bool bProcessWide = false;

        std::uint64_t Total() const { return RecordedOnContinue + RecordedOnTrap; }
    };

    namespace ContractDetail
    {
        // ---- atomic access to plain memory ------------------------------------
        //
        // The state block is mapped rather than constructed, so it cannot hold std::atomic: the
        // mapped pages must be usable as they arrive. Intrinsics over plain integers keep the
        // layout language-agnostic and the operations lock-free on every target here.

#if defined(_MSC_VER)
        inline std::uint32_t LoadAcquire32(const std::uint32_t* value)
        {
            return static_cast<std::uint32_t>(
                ::_InterlockedCompareExchange(const_cast<volatile long*>(reinterpret_cast<const volatile long*>(value)), 0, 0));
        }

        inline std::uint64_t LoadRelaxed64(const std::uint64_t* value)
        {
            return static_cast<std::uint64_t>(
                ::_InterlockedCompareExchange64(const_cast<volatile long long*>(reinterpret_cast<const volatile long long*>(value)), 0, 0));
        }

        inline void StoreRelease32(std::uint32_t* value, std::uint32_t next)
        {
            ::_InterlockedExchange(reinterpret_cast<volatile long*>(value), static_cast<long>(next));
        }

        inline void StoreRelaxed32(std::uint32_t* value, std::uint32_t next)
        {
            StoreRelease32(value, next);
        }

        inline void StoreRelaxed64(std::uint64_t* value, std::uint64_t next)
        {
            ::_InterlockedExchange64(reinterpret_cast<volatile long long*>(value), static_cast<long long>(next));
        }

        inline std::uint32_t CompareExchange32(std::uint32_t* value, std::uint32_t desired, std::uint32_t expected)
        {
            return static_cast<std::uint32_t>(::_InterlockedCompareExchange(
                reinterpret_cast<volatile long*>(value), static_cast<long>(desired), static_cast<long>(expected)));
        }

        inline std::uint32_t Increment32(std::uint32_t* value)
        {
            return static_cast<std::uint32_t>(::_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(value), 1));
        }

        inline void Increment64(std::uint64_t* value)
        {
            ::_InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(value), 1);
        }
#else
        inline std::uint32_t LoadAcquire32(const std::uint32_t* value)
        {
            return __atomic_load_n(value, __ATOMIC_ACQUIRE);
        }

        inline std::uint64_t LoadRelaxed64(const std::uint64_t* value)
        {
            return __atomic_load_n(value, __ATOMIC_RELAXED);
        }

        inline void StoreRelease32(std::uint32_t* value, std::uint32_t next)
        {
            __atomic_store_n(value, next, __ATOMIC_RELEASE);
        }

        inline void StoreRelaxed32(std::uint32_t* value, std::uint32_t next)
        {
            __atomic_store_n(value, next, __ATOMIC_RELAXED);
        }

        inline void StoreRelaxed64(std::uint64_t* value, std::uint64_t next)
        {
            __atomic_store_n(value, next, __ATOMIC_RELAXED);
        }

        inline std::uint32_t CompareExchange32(std::uint32_t* value, std::uint32_t desired, std::uint32_t expected)
        {
            std::uint32_t current = expected;
            __atomic_compare_exchange_n(value, &current, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
            return current;
        }

        inline std::uint32_t Increment32(std::uint32_t* value)
        {
            return __atomic_fetch_add(value, 1u, __ATOMIC_RELAXED);
        }

        inline void Increment64(std::uint64_t* value)
        {
            __atomic_fetch_add(value, 1ull, __ATOMIC_RELAXED);
        }
#endif

        // ---- the process-wide state -------------------------------------------

        inline ContractState& ModuleLocalState()
        {
            // Constant-initialised: reaching it takes no guard variable and no heap.
            static ContractState state;
            return state;
        }

        /**
         * One state block per process, not one per module. The engine DLL and the executable that
         * loads it each instantiate this header, so a plain function-local static would count every
         * violation twice - once in the module that reported it and once in the module that reads
         * the number - and the second copy would always read zero.
         *
         * The rendezvous is a pagefile-backed section whose name carries the process id: modules of
         * one process map the same pages, and two processes - the editor and a test run, or two
         * test runs - never share a count. A pagefile-backed section is session-wide by name, which
         * is exactly why the process id is part of it. The block is plain integers, so nothing has to
         * be constructed in it and no module owns it; a module that cannot map the section keeps its
         * own block instead, which still counts correctly for that module.
         */
        inline ContractState* MapProcessWideState()
        {
#if defined(_WIN32)
            // PAGE_READWRITE and FILE_MAP_ALL_ACCESS, spelled out because windows.h is not included.
            constexpr unsigned long kPageReadWrite = 0x04;
            constexpr unsigned long kFileMapAllAccess = 0x000F001F;

            wchar_t sectionName[64]{};
            std::swprintf(
                sectionName, sizeof(sectionName) / sizeof(sectionName[0]), L"Local\\minEngine-contract-state-%d",
                ::_getpid());

            const std::size_t size = sizeof(ContractState);
            void* section = ContractWin32::CreateFileMappingW(
                reinterpret_cast<void*>(-1), // INVALID_HANDLE_VALUE: backed by the paging file
                nullptr,
                kPageReadWrite,
                0,
                static_cast<unsigned long>(size),
                sectionName);
            if (section == nullptr)
            {
                return nullptr;
            }

            // The section handle is deliberately never closed: the mapping has to outlive every
            // module that reports, and it is released with the process.
            void* view = ContractWin32::MapViewOfFile(section, kFileMapAllAccess, 0, 0, size);
            if (view == nullptr)
            {
                return nullptr;
            }
            return static_cast<ContractState*>(view);
#else
            return nullptr;
#endif
        }

        struct ResolvedState
        {
            ContractState* Block = nullptr;
            bool bProcessWide = false;
        };

        inline ResolvedState ResolveState()
        {
            // Module-local once: the mapping call below is identical in every module, and the
            // section name is what makes them agree on one block.
            static std::uint32_t state = 0;
            static ContractState* block = nullptr;
            static bool bProcessWide = false;

            if (LoadAcquire32(&state) == 0)
            {
                if (CompareExchange32(&state, 1u, 0u) == 0)
                {
                    ContractState* mapped = MapProcessWideState();
                    block = mapped != nullptr ? mapped : &ModuleLocalState();
                    bProcessWide = mapped != nullptr;
                    StoreRelease32(&state, 2u);
                }
                else
                {
                    while (LoadAcquire32(&state) != 2u)
                    {
                        std::this_thread::yield();
                    }
                }
            }

            ResolvedState resolved;
            resolved.Block = block;
            resolved.bProcessWide = bProcessWide;
            return resolved;
        }

        inline ContractState& State()
        {
            return *ResolveState().Block;
        }

        // ---- identity of the reporting thread ---------------------------------

        inline ContractRoleResolver& RoleResolverSlot()
        {
            return State().RoleResolver;
        }

        /**
         * Installed by FrameThreading while an instance exists. A slot rather than an include keeps
         * this header leaf-level: FrameThreading.h reaches Core.h, which includes this file, so
         * depending on it here would be an include cycle decided by include order.
         */
        inline void SetContractRoleResolver(ContractRoleResolver resolver)
        {
            RoleResolverSlot() = resolver;
        }

        inline const char* CurrentRole()
        {
            const ContractRoleResolver resolver = RoleResolverSlot();
            if (resolver == nullptr)
            {
                return "unbound (no frame-role resolver installed)";
            }
            const char* role = resolver();
            return role != nullptr ? role : "unbound";
        }

        inline std::uint32_t CurrentLogThreadId()
        {
            return static_cast<std::uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }

        inline std::uint32_t& ReportDepth()
        {
            static thread_local std::uint32_t depth = 0;
            return depth;
        }

        // ---- text --------------------------------------------------------------

        inline const char* ShortFileName(const char* path)
        {
            if (path == nullptr)
            {
                return "(unknown)";
            }
            const char* name = path;
            for (const char* cursor = path; *cursor != '\0'; ++cursor)
            {
                if (*cursor == '\\' || *cursor == '/')
                {
                    name = cursor + 1;
                }
            }
            return name;
        }

        inline std::uint32_t Fnv1a(const char* text, std::uint32_t seed = 2166136261u)
        {
            std::uint32_t hash = seed;
            if (text == nullptr)
            {
                return hash;
            }
            for (const char* cursor = text; *cursor != '\0'; ++cursor)
            {
                hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(*cursor));
                hash *= 16777619u;
            }
            return hash;
        }

        /** Steps the budget of one distinct violation; -1 when the site already tracks too many. */
        inline int ClaimBudgetSlot(ContractSite& site, std::uint32_t key)
        {
            for (std::uint32_t index = 0; index < kContractBudgetedViolations; ++index)
            {
                const std::uint32_t known = LoadAcquire32(&site.BudgetKeys[index]);
                if (known == key)
                {
                    return static_cast<int>(index);
                }
                if (known == 0u && CompareExchange32(&site.BudgetKeys[index], key, 0u) == 0u)
                {
                    return static_cast<int>(index);
                }
            }
            return -1;
        }

        /**
         * Writes the line to the standard error descriptor, not through the C stream: the stream
         * carries a lock of its own, and the report path may not queue behind one.
         */
        inline void WriteToStandardError(const char* text)
        {
            const std::size_t length = std::strlen(text);
#if defined(_WIN32)
            ::_write(2, text, static_cast<unsigned>(length));
            ::_write(2, "\r\n", 2);
#else
            (void)::write(2, text, length);
            (void)::write(2, "\n", 1);
#endif
        }

        /** Copies one line into a free staging slot, or counts it when the staging area is full. */
        inline void StageRecord(ContractState& state, const char* text)
        {
            for (std::uint32_t attempt = 0; attempt < kContractRecordSlots; ++attempt)
            {
                const std::uint32_t index = Increment32(&state.NextSlot) % kContractRecordSlots;
                ContractRecordSlot& slot = state.Slots[index];
                if (CompareExchange32(&slot.State, 1u, 0u) != 0u)
                {
                    continue;
                }

                std::size_t copied = 0;
                for (; text[copied] != '\0' && copied + 1 < kContractRecordTextCapacity; ++copied)
                {
                    slot.Text[copied] = text[copied];
                }
                slot.Text[copied] = '\0';
                StoreRelease32(&slot.State, 2u);
                return;
            }
            Increment64(&state.Undelivered);
        }

        [[noreturn]] inline void TrapContractViolation()
        {
#if defined(_MSC_VER)
            __debugbreak();
#elif defined(__GNUC__)
            __builtin_trap();
#endif
            // Reached when a debugger resumed past the trap: a premise that no longer holds must not
            // be allowed to run on into the code that depends on it.
            std::abort();
        }
    }

    /** Installed by FrameThreading while a frame-role instance exists; cleared when it shuts down. */
    inline void SetContractRoleResolver(ContractRoleResolver resolver)
    {
        ContractDetail::SetContractRoleResolver(resolver);
    }

    inline ContractViolationCounters GetContractViolationCounters()
    {
        const ContractDetail::ResolvedState resolved = ContractDetail::ResolveState();
        const ContractState& state = *resolved.Block;

        ContractViolationCounters counters;
        counters.RecordedOnContinue = ContractDetail::LoadRelaxed64(&state.Ensured);
        counters.RecordedOnTrap = ContractDetail::LoadRelaxed64(&state.Trapped);
        counters.Suppressed = ContractDetail::LoadRelaxed64(&state.Suppressed);
        counters.Undelivered = ContractDetail::LoadRelaxed64(&state.Undelivered);
        counters.bProcessWide = resolved.bProcessWide;
        return counters;
    }

    /** Test support: the counters are process-wide, so only a test that owns the process resets them. */
    inline void ResetContractViolationCounters()
    {
        ContractState& state = ContractDetail::State();
        ContractDetail::StoreRelaxed64(&state.Ensured, 0u);
        ContractDetail::StoreRelaxed64(&state.Trapped, 0u);
        ContractDetail::StoreRelaxed64(&state.Suppressed, 0u);
        ContractDetail::StoreRelaxed64(&state.Undelivered, 0u);
    }

    /**
     * Records one contract violation and returns. Never throws, never blocks on a lock another
     * thread could be holding while it violates a contract, never allocates.
     */
    inline void ReportContractViolation(ContractSite& site, ContractGrade grade, const char* format, ...)
    {
        const char* userFormat = format != nullptr ? format : "(no message)";

        ContractState& state = ContractDetail::State();
        ContractDetail::Increment64(
            grade == ContractGrade::AssertFatal ? &state.Trapped : &state.Ensured);

        // A report raised from inside a report - a resolver or a formatter that faults, or a check
        // inside the report path itself - is counted but must not recurse into the same buffers.
        if (ContractDetail::ReportDepth() != 0)
        {
            return;
        }
        ContractDetail::ReportDepth() = 1;

        char userMessage[kContractRecordTextCapacity]{};
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(userMessage, sizeof(userMessage), userFormat, arguments);
        va_end(arguments);

        const char* role = ContractDetail::CurrentRole();
        const std::uint32_t threadId = ContractDetail::CurrentLogThreadId();
        const char* kind = "ME_ENSURE";
        if (grade == ContractGrade::AssertFatal)
        {
            kind = "ME_ASSERT";
        }
        else if (grade == ContractGrade::AssertDegraded)
        {
            kind = "ME_ASSERT (this build records instead of trapping)";
        }
        const char* fileName = ContractDetail::ShortFileName(site.File);

        std::uint32_t key = ContractDetail::Fnv1a(userMessage);
        key = ContractDetail::Fnv1a(role, key) | 1u;

        const int budgetSlot = ContractDetail::ClaimBudgetSlot(site, key);
        std::uint32_t reported = 0;
        if (budgetSlot >= 0)
        {
            reported = ContractDetail::Increment32(&site.BudgetUsed[budgetSlot]);
        }

        // A site that has spent its budget, or one that already tracks too many distinct violations,
        // only counts. The failure is not lost: the counter keeps rising and the frame summary
        // reports it.
        const bool bOutOfBudget =
            budgetSlot < 0 || reported > kContractVerboseReportsPerViolation;
        if (bOutOfBudget)
        {
            ContractDetail::Increment64(&state.Suppressed);
            ContractDetail::ReportDepth() = 0;
            return;
        }

        char text[kContractRecordTextCapacity]{};
        if (reported == kContractVerboseReportsPerViolation)
        {
            // The last line this violation will write on its own: silence after it would read as a
            // fix, and the reader has to know that the count keeps rising instead.
            ContractDetail::Increment64(&state.Suppressed);
            std::snprintf(
                text,
                sizeof(text),
                "%s contract violation at %s:%d (thread %u, role %s): %s - %u reports is this "
                "site's budget, later failures are counted but not logged",
                kind,
                fileName,
                site.Line,
                threadId,
                role,
                userMessage,
                kContractVerboseReportsPerViolation);
        }
        else
        {
            std::snprintf(
                text,
                sizeof(text),
                "%s contract violation at %s:%d (thread %u, role %s): %s",
                kind,
                fileName,
                site.Line,
                threadId,
                role,
                userMessage);
        }

        // The standard error mirror follows the same budget as the staged record: a site that has
        // gone quiet must not keep painting the console either.
        ContractDetail::WriteToStandardError(text);
        ContractDetail::StageRecord(state, text);
        ContractDetail::ReportDepth() = 0;
    }
}

// One report site per expansion: the file and line are baked in, and the report budget lives with
// the site, so a site that fires every frame writes a bounded number of lines.
#define ME_CONTRACT_REPORT(Grade, ...)                                                              \
    do                                                                                              \
    {                                                                                               \
        static ::minEngine::ContractSite meContractSite{__FILE__, __LINE__};                        \
        ::minEngine::ReportContractViolation(meContractSite, (Grade), __VA_ARGS__);                 \
    } while (0)

#define ME_ENSURE(condition, ...)                                                                   \
    do                                                                                              \
    {                                                                                               \
        if (!(condition))                                                                           \
        {                                                                                           \
            ME_CONTRACT_REPORT(::minEngine::ContractGrade::Ensure, __VA_ARGS__);                    \
        }                                                                                           \
    } while (0)

#if ME_CONTRACT_ASSERT_FATAL
    #define ME_ASSERT(condition, ...)                                                               \
        do                                                                                          \
        {                                                                                           \
            if (!(condition))                                                                       \
            {                                                                                       \
                ME_CONTRACT_REPORT(::minEngine::ContractGrade::AssertFatal, __VA_ARGS__);           \
                ::minEngine::ContractDetail::TrapContractViolation();                               \
            }                                                                                       \
        } while (0)
#else
    // Release grade: one violated contract is a record and a count, never a dead build. The report
    // keeps the grade, so a reader sees an assert that only recorded.
    #define ME_ASSERT(condition, ...)                                                               \
        do                                                                                          \
        {                                                                                           \
            if (!(condition))                                                                       \
            {                                                                                       \
                ME_CONTRACT_REPORT(::minEngine::ContractGrade::AssertDegraded, __VA_ARGS__);        \
            }                                                                                       \
        } while (0)
#endif
