#pragma once

#include "Core.h"

namespace minEngine
{
    constexpr uint32_t kDefaultMaxUndoStackDepth = 100;
    constexpr uint32_t kMinMaxUndoStackDepth = 16;
    constexpr uint32_t kMaxMaxUndoStackDepth = 4096;

    uint32_t ResolveMaxUndoStackDepth(uint32_t projectSettingValue);
}
