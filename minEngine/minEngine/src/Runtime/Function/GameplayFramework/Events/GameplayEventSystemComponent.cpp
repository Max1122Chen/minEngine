#include "Runtime/Function/GameplayFramework/Events/GameplayEventSystemComponent.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"
#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "Core.h"

#include <algorithm>

namespace minEngine
{
    GameplayEventSystemComponent::GameplayEventSystemComponent()
    {
        m_bCanEverTick = false;
    }

    GameplayEventSystemComponent::~GameplayEventSystemComponent()
    {
        ClearListeners();
    }

    GameplayEventChannel GameplayEventSystemComponent::Channel(const GameplayTag& tag) const
    {
        ME_ASSERT(GameplayTagManager::HasInstance(), "GameplayTagManager required for GameplayEvent channels");
        ME_ASSERT(GameplayTagManager::Get().IsValidTag(tag), "Invalid channel GameplayTag");

        GameplayEventChannel channel;
        channel.Tag = tag;
        return channel;
    }

    GameplayEventChannel GameplayEventSystemComponent::GetDefaultChannel() const
    {
        return Channel(TAG_Channel_Default);
    }

    void GameplayEventSystemComponent::Dispatch(const GameplayEvent& event)
    {
        DispatchOnChannel(GetDefaultChannel(), event);
    }

    void GameplayEventSystemComponent::Dispatch(const GameplayEventChannel& channel, const GameplayEvent& event)
    {
        DispatchOnChannel(channel, event);
    }

    std::string GameplayEventSystemComponent::Subscribe(const GameplayEventSubscribeOptions& options)
    {
        ME_ASSERT(options.Handler != nullptr, "GameplayEvent subscribe requires a handler");
        ME_ASSERT(options.Channel.Tag.IsValid(), "GameplayEvent subscribe requires a valid channel tag");

        std::string listenerId = options.ListenerId;
        if (listenerId.empty())
        {
            listenerId = "listener-" + std::to_string(m_NextListenerSequence++);
        }

        if (m_Listeners.find(listenerId) != m_Listeners.end())
        {
            ME_CORE_ERROR("GameplayEvent listener id already registered: {}", listenerId);
            ME_ASSERT(false, "Duplicate GameplayEvent listener id");
            return {};
        }

        ListenerRecord record;
        record.ListenerId = listenerId;
        record.ChannelIndex = options.Channel.Tag.GetIndex();
        record.Priority = options.Priority;
        record.RegistrationIndex = m_NextRegistrationIndex++;
        record.RequiredAll = options.RequiredAll;
        record.RequiredAny = options.RequiredAny;
        record.Handler = options.Handler;

        m_ListenerIdsByChannel[record.ChannelIndex].push_back(listenerId);
        m_Listeners.emplace(listenerId, std::move(record));
        return listenerId;
    }

    bool GameplayEventSystemComponent::Unsubscribe(const std::string& listenerId)
    {
        const auto it = m_Listeners.find(listenerId);
        if (it == m_Listeners.end())
        {
            return false;
        }

        const uint32_t channelIndex = it->second.ChannelIndex;
        auto channelIt = m_ListenerIdsByChannel.find(channelIndex);
        if (channelIt != m_ListenerIdsByChannel.end())
        {
            auto& ids = channelIt->second;
            ids.erase(std::remove(ids.begin(), ids.end(), listenerId), ids.end());
        }

        m_Listeners.erase(it);
        return true;
    }

    void GameplayEventSystemComponent::SetMaxDispatchDepth(int32_t depth)
    {
        ME_ASSERT(depth > 0, "maxDispatchDepth must be positive");
        m_MaxDispatchDepth = depth;
    }

    Scene* GameplayEventSystemComponent::GetOwningScene() const
    {
        if (m_Owner == nullptr)
        {
            return nullptr;
        }

        const MEObject* outer = m_Owner->GetOuter();
        if (outer == nullptr || !outer->IsA(Scene::StaticClass()))
        {
            return nullptr;
        }

        return const_cast<Scene*>(static_cast<const Scene*>(outer));
    }

    void GameplayEventSystemComponent::OnActivate()
    {
        if (Scene* scene = GetOwningScene())
        {
            scene->RegisterGameplayEventSystem(this);
        }
    }

    void GameplayEventSystemComponent::OnDeactivate()
    {
        if (Scene* scene = GetOwningScene())
        {
            scene->UnregisterGameplayEventSystem(this);
        }
        ClearListeners();
    }

    void GameplayEventSystemComponent::DispatchOnChannel(
        const GameplayEventChannel& channel,
        const GameplayEvent& event)
    {
        if (m_DispatchDepth >= m_MaxDispatchDepth)
        {
            ME_CORE_ERROR(
                "GameplayEvent dispatch depth exceeded maxDispatchDepth={}",
                m_MaxDispatchDepth);
            ME_ASSERT(false, "GameplayEvent dispatch depth exceeded");
            return;
        }

        ++m_DispatchDepth;

        GameplayEvent snapshot(event.Tags.GetManager());
        snapshot.Tags = event.Tags.Clone();

        std::vector<ListenerRecord> matched;
        const auto channelIt = m_ListenerIdsByChannel.find(channel.Tag.GetIndex());
        if (channelIt != m_ListenerIdsByChannel.end())
        {
            matched.reserve(channelIt->second.size());
            for (const std::string& listenerId : channelIt->second)
            {
                const auto listenerIt = m_Listeners.find(listenerId);
                if (listenerIt == m_Listeners.end())
                {
                    continue;
                }
                if (MatchesListener(snapshot, listenerIt->second))
                {
                    matched.push_back(listenerIt->second);
                }
            }
        }

        std::stable_sort(matched.begin(), matched.end(), CompareListeners);

        for (const ListenerRecord& listener : matched)
        {
            listener.Handler(snapshot);
        }

        --m_DispatchDepth;
    }

    bool GameplayEventSystemComponent::MatchesListener(
        const GameplayEvent& event,
        const ListenerRecord& listener) const
    {
        if (!listener.RequiredAll.empty() && !event.Tags.HasAll(listener.RequiredAll))
        {
            return false;
        }

        if (!listener.RequiredAny.empty() && !event.Tags.HasAny(listener.RequiredAny))
        {
            return false;
        }

        return true;
    }

    bool GameplayEventSystemComponent::CompareListeners(
        const ListenerRecord& left,
        const ListenerRecord& right)
    {
        if (left.Priority != right.Priority)
        {
            return left.Priority > right.Priority;
        }
        return left.RegistrationIndex < right.RegistrationIndex;
    }

    void GameplayEventSystemComponent::ClearListeners()
    {
        m_Listeners.clear();
        m_ListenerIdsByChannel.clear();
    }
}
