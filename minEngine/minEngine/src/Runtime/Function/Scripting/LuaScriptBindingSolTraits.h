#pragma once

#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <type_traits>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

// GameObject/Component are not Lua containers; without this, sol may treat
// GameObject* returns as container pushes when the usertype is not visible yet.
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
}
