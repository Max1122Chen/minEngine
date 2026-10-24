#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace minEngine
{
    /** Opaque id for a task accepted by TaskSystem::Submit. Zero means invalid / rejected. */
    struct MINENGINE_API TaskHandle
    {
        uint64_t Id = 0;

        bool IsValid() const { return Id != 0; }

        bool operator==(const TaskHandle& other) const { return Id == other.Id; }
        bool operator!=(const TaskHandle& other) const { return Id != other.Id; }
    };
}
