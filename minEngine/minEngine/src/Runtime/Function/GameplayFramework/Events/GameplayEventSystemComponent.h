#pragma once

#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEvent.h"
#include "Runtime/Function/GameplayFramework/Events/GameplayEventChannel.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class Scene;

    struct GameplayEventSubscribeOptions
    {
        GameplayEventChannel Channel;
        std::string ListenerId;
        int32_t Priority = 0;
        std::vector<GameplayTag> RequiredAll;
        std::vector<GameplayTag> RequiredAny;
        std::function<void(const GameplayEvent&)> Handler;
    };

    ME_CLASS()
    class GameplayEventSystemComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        GameplayEventSystemComponent();
        virtual ~GameplayEventSystemComponent() override;

        GameplayEventChannel Channel(const GameplayTag& tag) const;
        GameplayEventChannel GetDefaultChannel() const;

        void Dispatch(const GameplayEvent& event);
        void Dispatch(const GameplayEventChannel& channel, const GameplayEvent& event);

        std::string Subscribe(const GameplayEventSubscribeOptions& options);
        bool Unsubscribe(const std::string& listenerId);

        int32_t GetMaxDispatchDepth() const { return m_MaxDispatchDepth; }
        void SetMaxDispatchDepth(int32_t depth);

        Scene* GetOwningScene() const;

    protected:
        void OnActivate() override;
        void OnDeactivate() override;

    private:
        struct ListenerRecord
        {
            std::string ListenerId;
            uint32_t ChannelIndex = UINT32_MAX;
            int32_t Priority = 0;
            uint64_t RegistrationIndex = 0;
            std::vector<GameplayTag> RequiredAll;
            std::vector<GameplayTag> RequiredAny;
            std::function<void(const GameplayEvent&)> Handler;
        };

        void DispatchOnChannel(const GameplayEventChannel& channel, const GameplayEvent& event);
        bool MatchesListener(const GameplayEvent& event, const ListenerRecord& listener) const;
        static bool CompareListeners(const ListenerRecord& left, const ListenerRecord& right);
        void ClearListeners();

        int32_t m_MaxDispatchDepth = 16;
        int32_t m_DispatchDepth = 0;
        uint64_t m_NextRegistrationIndex = 0;
        uint64_t m_NextListenerSequence = 0;
        std::unordered_map<std::string, ListenerRecord> m_Listeners;
        std::unordered_map<uint32_t, std::vector<std::string>> m_ListenerIdsByChannel;
    };
}

#include "Generated/Reflection/GameplayEventSystemComponent.gen.h"
