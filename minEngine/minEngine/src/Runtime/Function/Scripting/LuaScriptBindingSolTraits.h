#pragma once

#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/LuaComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <type_traits>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

// Engine object types are not Lua containers; without this, sol may treat
// pointer returns as container pushes when the usertype is not visible yet.
namespace sol
{
    template <>
    struct is_container<minEngine::GameObject> : std::false_type
    {
    };

    template <>
    struct is_container<minEngine::Component> : std::false_type
    {
    };

    template <>
    struct is_container<minEngine::LuaComponent> : std::false_type
    {
    };
}
