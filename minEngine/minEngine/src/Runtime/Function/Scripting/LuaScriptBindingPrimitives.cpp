#include "LuaScriptBindingPrimitives.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Audio/Components/AudioComponent.h"
#include "Runtime/Function/Framework/Components/Component.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace minEngine
{
    void RegisterLuaScriptBindingPrimitives(sol::state& state)
    {
        // Hand-written math POD bindings (CORE-F02-S05). Not ME_STRUCT/ScriptType —
        // VectorN are glm typedefs under minEngine::Math; wrapping them is a separate domain.

        const auto isUsertypeRegistered = [&state](const char* name) -> bool
        {
            const sol::object existing = state[name];
            const sol::type type = existing.get_type();
            return type == sol::type::userdata || type == sol::type::table;
        };

        if (!isUsertypeRegistered("Vector2"))
        {
            state.new_usertype<Vector2>(
                "Vector2",
                sol::constructors<Vector2(), Vector2(float, float)>(),
                "x",
                &Vector2::x,
                "y",
                &Vector2::y);
        }

        if (!isUsertypeRegistered("Vector3"))
        {
            state.new_usertype<Vector3>(
                "Vector3",
                sol::constructors<Vector3(), Vector3(float, float, float)>(),
                "x",
                &Vector3::x,
                "y",
                &Vector3::y,
                "z",
                &Vector3::z);
        }

        if (!isUsertypeRegistered("Vector4"))
        {
            state.new_usertype<Vector4>(
                "Vector4",
                sol::constructors<Vector4(), Vector4(float, float, float, float)>(),
                "x",
                &Vector4::x,
                "y",
                &Vector4::y,
                "z",
                &Vector4::z,
                "w",
                &Vector4::w);
        }

        state.set_function(
            "AsAudioComponent",
            [](Component* component) -> AudioComponent*
            {
                if (component == nullptr || component->GetClass() == nullptr
                    || !component->IsA(AudioComponent::StaticClass()))
                {
                    return nullptr;
                }

                return static_cast<AudioComponent*>(component);
            });
    }
}
