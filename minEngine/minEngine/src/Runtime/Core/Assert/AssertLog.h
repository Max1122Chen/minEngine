#pragma once

#include "Runtime/Core/Assert/Assert.h"

namespace minEngine
{
    /** What one frame boundary published, and the totals the frame summary is built from. */
    struct ContractViolationFrameSample
    {
        /** Records this call turned into log lines. */
        std::uint32_t Published = 0;

        /** The counters as they stood when this call returned, that is, after publishing. */
        ContractViolationCounters Counters;
    };

    /**
     * Publishes every contract record staged since the previous call - one Error log record each -
     * and samples the counters in the same call.
     *
     * Call this from a thread that owns no log lock: the frame boundary is the intended one
     * (Engine::ReportFrameWorkLoss, reached by Engine::Run and by Engine::CompleteFrameReleases).
     * The report path in Assert.h cannot publish by itself: LogSystem::Emit calls every sink while
     * holding the log mutex, so a violation raised from inside a sink is already holding it, and a
     * report that asked for it again would hang the process instead of recording the violation.
     *
     * The two halves are one call because they are one moment. A boundary that published and then
     * read the counters elsewhere could describe a frame with records the log had not received
     * yet, and a summary that counts what the log is missing is how a reader learns to distrust
     * both. Exported because the staging area and the counters belong to the engine module, while
     * the frame boundary may be driven by the host: the editor calls Engine::CompleteFrameReleases
     * for every frame it draws and never enters Engine::Run.
     */
    MINENGINE_API ContractViolationFrameSample PublishAndSampleContractViolations();
}
