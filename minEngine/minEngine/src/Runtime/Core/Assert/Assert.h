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
 *     No allocation, no formatting through the C++ library, no log-system lock, no lock at all.
 *     Publishing the staged records to the log happens later, at the frame boundary
 *     (PublishAndSampleContractViolations in AssertLog.h), from a caller that owns no log lock.
 *
 * Where the state lives
 *     The counters and the staging area are one block in the engine module's data (AssertLog.cpp),
 *     and the accessors below are the only way to it. One block, not one per module: the numbers a
 *     report moves and the numbers a host prints have to be the same numbers, and a reporter in the
 *     executable, in a game module or in the editor DLL is reporting about the same process. The
 *     functions are ordinary exports, so a call is a call - no allocation, no lock, no lazy
 *     initialisation behind them - and the record path itself stays inline here, because it is the
 *     part that has to run wherever the violation is detected.
 *
 * Rate limiting
 *     A violated contract is usually violated every frame, so a site that logged once per failure
 *     would bury the log it is trying to inform. Each site keeps a small budget table keyed on
 *     (message, role): the first kContractVerboseReportsPerViolation failures of a distinct
 *     violation are recorded in full, the next one states that the site has gone quiet, and any
 *     failure after that only moves the counters. Nothing is lost silently: the cumulative total
 *     keeps rising and the per-frame summary reports it. The budget table belongs to the site, so
 *     it stays with the site's expansion instead of crossing a module boundary.
 */

#include "Runtime/Core/EngineAPI.h"

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

namespace minEngine
{
    /** Text capacity of one staged record: a file:line, a thread, a role and the author's message. */
    constexpr std::uint32_t kContractRecordTextCapacity = 256;

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
     *
     * The budget table lives with the site: it belongs to this expansion, only threads that reach
     * this expansion touch it, and keeping it here is what lets the table be found without a lookup
     * on the failure path.
     */
    struct ContractSite
    {
        const char* File = nullptr;
        int Line = 0;
        std::uint32_t BudgetKeys[kContractBudgetedViolations]{};
        std::uint32_t BudgetUsed[kContractBudgetedViolations]{};
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

        std::uint64_t Total() const { return RecordedOnContinue + RecordedOnTrap; }
    };

    // ---- the one state block, reached through the engine module ---------------
    //
    // Every function below is public API for the same reason: the state they touch belongs to the
    // process, while the code that has to touch it is spread over the modules of that process. A
    // report is raised wherever the contract is broken, a summary is printed by whichever module
    // drives frames, and a diagnostic is read by whoever runs the process - all three must mean the
    // same counters, and none of them may acquire a lock or allocate while doing it.

    /**
     * Counts one report on the failing thread: one increment for its grade, and one more for the
     * suppression when a site's budget kept it out of the log. Called from the inline record path,
     * so it must stay allocation-free and lock-free.
     */
    MINENGINE_API void CountContractViolation(ContractGrade grade, bool bSuppressed);

    /**
     * Copies one already-formatted line into the staging area, or counts it as undelivered when the
     * area is full. Never allocates and never waits, so the caller may hold any lock - including the
     * log system's own.
     */
    MINENGINE_API void StageContractRecord(const char* text);

    /**
     * Installs the resolver a report asks for its thread's frame role, or clears it with null. The
     * role table belongs to a frame-role instance, which a host, the editor or a game module may
     * own; the block that remembers the resolver belongs to the engine module, so a report raised
     * anywhere names the role its thread actually holds.
     */
    MINENGINE_API void SetContractRoleResolver(ContractRoleResolver resolver);

    /**
     * The frame role the calling thread holds, as a string the engine module owns, or a sentence
     * saying that no resolver is installed. The record has to name the role of the thread that
     * broke the contract, and that thread is whichever one detected the violation.
     */
    MINENGINE_API const char* CurrentContractRole();

    /**
     * One read of every counter: what a diagnostic prints, what a host or a test checks, and what
     * the per-frame summary is built from. The counters are the process's, so the read has to be
     * one call into the module that owns them rather than a copy per module.
     */
    MINENGINE_API ContractViolationCounters GetContractViolationCounters();

    namespace ContractDetail
    {
        // ---- atomic access to plain memory ------------------------------------
        //
        // The state block and the per-site budget tables are plain integers touched with
        // interlocked intrinsics: the report path may not construct anything, and a table that a
        // thread claims a slot in has to survive a thread that faults in the middle of a report.

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

    /**
     * Records one contract violation and returns. Never throws, never blocks on a lock another
     * thread could be holding while it violates a contract, never allocates.
     *
     * The formatting, the site's budget and the standard error mirror stay here, in the module that
     * detected the violation; the counters and the staged record go where every module can see the
     * same ones.
     */
    inline void ReportContractViolation(ContractSite& site, ContractGrade grade, const char* format, ...)
    {
        const char* userFormat = format != nullptr ? format : "(no message)";

        // A report raised from inside a report - a resolver or a formatter that faults, or a check
        // inside the report path itself - is counted but must not recurse into the same buffers.
        if (ContractDetail::ReportDepth() != 0)
        {
            CountContractViolation(grade, /*bSuppressed=*/false);
            return;
        }
        ContractDetail::ReportDepth() = 1;

        char userMessage[kContractRecordTextCapacity]{};
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(userMessage, sizeof(userMessage), userFormat, arguments);
        va_end(arguments);

        const char* role = CurrentContractRole();
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
        // reports it. The last line a violation writes on its own is the announcement that the
        // budget is spent, and that line is a suppression too - silence after it would read as a fix.
        const bool bOutOfBudget = budgetSlot < 0 || reported > kContractVerboseReportsPerViolation;
        const bool bBudgetAnnouncement = !bOutOfBudget && reported == kContractVerboseReportsPerViolation;
        CountContractViolation(grade, bOutOfBudget || bBudgetAnnouncement);

        if (bOutOfBudget)
        {
            ContractDetail::ReportDepth() = 0;
            return;
        }

        char text[kContractRecordTextCapacity]{};
        if (bBudgetAnnouncement)
        {
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
        StageContractRecord(text);
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
