#pragma once

#include "Core.h"

#include <string_view>

namespace minEngine
{
    class MEObject;

    // Tactical bridge until BUG-CORE-001 (unified property Assign) lands on master.
    // Call after Inspector / undo writes a reflected field directly.
    void ApplyPhysicsEditorSideEffects(MEObject* owner, std::string_view propertyPath);
}
