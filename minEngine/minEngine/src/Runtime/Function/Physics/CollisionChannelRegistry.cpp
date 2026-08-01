#include "PhysicsTypes.h"

#include <algorithm>
#include <cctype>

namespace minEngine
{
    namespace
    {
        uint8_t ToIndex(ECollisionChannel channel)
        {
            return static_cast<uint8_t>(channel);
        }

        bool IsValidChannel(ECollisionChannel channel)
        {
            return channel < ECollisionChannel::MAX;
        }

        std::string ToLowerAscii(std::string_view text)
        {
            std::string result(text);
            std::transform(result.begin(), result.end(), result.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            return result;
        }
    }

    CollisionChannelRegistry& CollisionChannelRegistry::Get()
    {
        static CollisionChannelRegistry instance;
        return instance;
    }

    CollisionChannelRegistry::CollisionChannelRegistry()
    {
        ResetToDefaults();
    }

    void CollisionChannelRegistry::ResetToDefaults()
    {
        for (uint8_t row = 0; row < ChannelCount; ++row)
        {
            for (uint8_t column = 0; column < ChannelCount; ++column)
            {
                m_Matrix[row][column] = ECollisionResponse::Block;
            }
        }

        const auto setSymmetric = [this](ECollisionChannel a, ECollisionChannel b, ECollisionResponse response)
        {
            m_Matrix[ToIndex(a)][ToIndex(b)] = response;
            m_Matrix[ToIndex(b)][ToIndex(a)] = response;
        };

        setSymmetric(ECollisionChannel::WorldStatic, ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
        setSymmetric(ECollisionChannel::WorldStatic, ECollisionChannel::Default, ECollisionResponse::Block);
        setSymmetric(ECollisionChannel::WorldStatic, ECollisionChannel::Trigger, ECollisionResponse::Overlap);
        setSymmetric(ECollisionChannel::Default, ECollisionChannel::Default, ECollisionResponse::Block);
        setSymmetric(ECollisionChannel::Default, ECollisionChannel::Trigger, ECollisionResponse::Overlap);
        setSymmetric(ECollisionChannel::Trigger, ECollisionChannel::Trigger, ECollisionResponse::Ignore);

        m_Names[ToIndex(ECollisionChannel::WorldStatic)] = "WorldStatic";
        m_Names[ToIndex(ECollisionChannel::Default)] = "Default";
        m_Names[ToIndex(ECollisionChannel::Trigger)] = "Trigger";
        m_Names[ToIndex(ECollisionChannel::Visibility)] = "Visibility";
        for (uint8_t index = ToIndex(ECollisionChannel::GameChannel1);
             index <= ToIndex(ECollisionChannel::GameChannel8);
             ++index)
        {
            m_Names[index] = "GameChannel" + std::to_string(index - ToIndex(ECollisionChannel::GameChannel1) + 1);
        }
    }

    ECollisionResponse CollisionChannelRegistry::GetResponse(ECollisionChannel a, ECollisionChannel b) const
    {
        if (!IsValidChannel(a) || !IsValidChannel(b))
        {
            return ECollisionResponse::Ignore;
        }

        return m_Matrix[ToIndex(a)][ToIndex(b)];
    }

    void CollisionChannelRegistry::SetResponse(
        ECollisionChannel a,
        ECollisionChannel b,
        ECollisionResponse response)
    {
        if (!IsValidChannel(a) || !IsValidChannel(b))
        {
            return;
        }

        m_Matrix[ToIndex(a)][ToIndex(b)] = response;
        m_Matrix[ToIndex(b)][ToIndex(a)] = response;
    }

    std::string_view CollisionChannelRegistry::GetChannelName(ECollisionChannel channel) const
    {
        if (!IsValidChannel(channel))
        {
            return {};
        }

        return m_Names[ToIndex(channel)];
    }

    bool CollisionChannelRegistry::TryFindChannelByName(
        std::string_view name,
        ECollisionChannel& outChannel) const
    {
        const std::string needle = ToLowerAscii(name);
        for (uint8_t index = 0; index < ChannelCount; ++index)
        {
            if (ToLowerAscii(m_Names[index]) == needle)
            {
                outChannel = static_cast<ECollisionChannel>(index);
                return true;
            }
        }

        return false;
    }

    void CollisionChannelRegistry::SetChannelName(ECollisionChannel channel, std::string name)
    {
        if (!IsValidChannel(channel) || name.empty())
        {
            return;
        }

        m_Names[ToIndex(channel)] = std::move(name);
    }
}
