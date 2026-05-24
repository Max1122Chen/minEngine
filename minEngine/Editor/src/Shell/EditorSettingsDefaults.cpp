#include "Shell/EditorSettingsDefaults.h"

#include <algorithm>

namespace minEngine
{
    uint32_t ResolveMaxUndoStackDepth(uint32_t projectSettingValue)
    {
        const uint32_t requested =
            projectSettingValue != 0 ? projectSettingValue : kDefaultMaxUndoStackDepth;
        return std::clamp(requested, kMinMaxUndoStackDepth, kMaxMaxUndoStackDepth);
    }
}
