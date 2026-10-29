// The shipped-build grade of ME_ASSERT, spelled out: the same policy a build with NDEBUG selects,
// so the release behaviour is exercised by the debug contract build as well.
#define ME_CONTRACT_ASSERT_FATAL 0

#include "Runtime/Core/Assert/Assert.h"

#include "ReleaseGradeProbe.h"

namespace contractprobe
{
    bool RunReleaseGradeAssertViolation()
    {
        ME_ASSERT(false, "release-grade ME_ASSERT: recorded, not trapped");
        return true;
    }
}
