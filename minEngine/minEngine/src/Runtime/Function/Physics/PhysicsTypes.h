#pragma once

#include "Core.h"
#include "Runtime/Core/Reflection/ReflectionAnnotations.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace minEngine
{
    using PhysicsBodyId = uint32_t;

    inline constexpr PhysicsBodyId InvalidPhysicsBodyId = UINT32_MAX;

    ME_ENUM()
    enum class EBodyType : uint8_t
    {
        Static = 0,
        Dynamic,
        Kinematic,
    };

    /** Aligns with UE ETeleportType — authority Transform changes only (not simulation writeback). */
    ME_ENUM()
    enum class ETeleportType : uint8_t
    {
        None = 0,
        TeleportPhysics,
        ResetPhysics,
    };

    /**
     * UE-style single channel enum: Object usage (collider identity) and Trace usage (queries, S03)
     * are two faces of the same type. GameChannel1–8 are reserved for project naming via registry.
     */
    ME_ENUM()
    enum class ECollisionChannel : uint8_t
    {
        WorldStatic = 0,
        Default,
        Trigger,
        Visibility,
        GameChannel1,
        GameChannel2,
        GameChannel3,
        GameChannel4,
        GameChannel5,
        GameChannel6,
        GameChannel7,
        GameChannel8,
        MAX,
    };

    ME_ENUM()
    enum class ECollisionResponse : uint8_t
    {
        Ignore = 0,
        Overlap,
        Block,
    };

    ME_ENUM()
    enum class EContactPhase : uint8_t
    {
        Begin = 0,
        End,
    };

    struct FPhysicsContactEvent
    {
        PhysicsBodyId BodyA{InvalidPhysicsBodyId};
        PhysicsBodyId BodyB{InvalidPhysicsBodyId};
        ECollisionResponse Response{ECollisionResponse::Block};
        EContactPhase Phase{EContactPhase::Begin};
    };

    class CollisionChannelRegistry
    {
    public:
        static CollisionChannelRegistry& Get();

        ECollisionResponse GetResponse(ECollisionChannel a, ECollisionChannel b) const;
        void SetResponse(ECollisionChannel a, ECollisionChannel b, ECollisionResponse response);

        std::string_view GetChannelName(ECollisionChannel channel) const;
        bool TryFindChannelByName(std::string_view name, ECollisionChannel& outChannel) const;
        void SetChannelName(ECollisionChannel channel, std::string name);

        void ResetToDefaults();

    private:
        CollisionChannelRegistry();

        static constexpr uint8_t ChannelCount =
            static_cast<uint8_t>(ECollisionChannel::MAX);

        ECollisionResponse m_Matrix[ChannelCount][ChannelCount]{};
        std::string m_Names[ChannelCount]{};
    };
}

#include "Generated/Reflection/PhysicsTypes.gen.h"
