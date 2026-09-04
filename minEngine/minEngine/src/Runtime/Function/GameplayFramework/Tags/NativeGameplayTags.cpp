#include "Runtime/Function/GameplayFramework/Tags/NativeGameplayTags.h"

#include "Runtime/Function/GameplayFramework/Tags/GameplayTagManager.h"

#include "Core.h"

namespace minEngine
{
    std::vector<std::string>& NativeGameplayTagRegistrar::GetNameStorage()
    {
        static std::vector<std::string> s_Names;
        return s_Names;
    }

    std::mutex& NativeGameplayTagRegistrar::GetMutex()
    {
        static std::mutex s_Mutex;
        return s_Mutex;
    }

    NativeGameplayTagRegistrar::NativeGameplayTagRegistrar(const char* tagName)
    {
        ME_ASSERT(tagName != nullptr && tagName[0] != '\0', "NativeGameplayTag name must be non-empty");

        std::lock_guard<std::mutex> lock(GetMutex());
        std::vector<std::string>& names = GetNameStorage();
        for (const std::string& existing : names)
        {
            if (existing == tagName)
            {
                return;
            }
        }
        names.emplace_back(tagName);
    }

    std::vector<std::string> NativeGameplayTagRegistrar::CopyRegisteredNames()
    {
        std::lock_guard<std::mutex> lock(GetMutex());
        return GetNameStorage();
    }

    NativeGameplayTag::NativeGameplayTag(const char* tagName)
        : m_TagName(tagName)
    {
        (void)NativeGameplayTagRegistrar(tagName);
    }

    GameplayTag NativeGameplayTag::GetTag() const
    {
        ME_ASSERT(GameplayTagManager::HasInstance(), "GameplayTagManager must be initialized before resolving native tags");
        return GameplayTagManager::Get().Resolve(m_TagName);
    }
}

ME_DEFINE_GAMEPLAY_TAG(TAG_Channel_Default, "Channel.Default");
