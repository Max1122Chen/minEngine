#pragma once

namespace contractprobe
{
    /**
     * Fires one ME_ASSERT from a translation unit built with the release grade, i.e. with the policy
     * a shipped build selects. Returns true when execution reached the line after the violation.
     */
    bool RunReleaseGradeAssertViolation();
}
