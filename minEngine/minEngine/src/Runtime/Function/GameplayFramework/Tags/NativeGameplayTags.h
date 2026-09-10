#pragma once

#include "Runtime/Function/GameplayFramework/Tags/GameplayTag.h"

#include <mutex>
#include <string>
#include <vector>

namespace minEngine
{
    /// Collects tag names registered via ME_DEFINE_GAMEPLAY_TAG* before Manager init.
    class NativeGameplayTagRegistrar
    {
    public:
        explicit NativeGameplayTagRegistrar(const char* tagName);

        static std::vector<std::string> CopyRegisteredNames();

    private:
        static std::vector<std::string>& GetNameStorage();
        static std::mutex& GetMutex();
    };

    /// Lazy handle that resolves against GameplayTagManager after Initialize.
    class NativeGameplayTag
    {
    public:
        explicit NativeGameplayTag(const char* tagName);

        const char* GetTagName() const { return m_TagName; }
        GameplayTag GetTag() const;
        operator GameplayTag() const { return GetTag(); }

    private:
        const char* m_TagName = nullptr;
    };
}

/// Declare a native gameplay tag defined in a .cpp (UE_DECLARE_GAMEPLAY_TAG_EXTERN analogue).
#define ME_DECLARE_GAMEPLAY_TAG_EXTERN(TagName) extern ::minEngine::NativeGameplayTag TagName

/// Define and register a native gameplay tag (UE_DEFINE_GAMEPLAY_TAG analogue). Use in .cpp only.
#define ME_DEFINE_GAMEPLAY_TAG(TagName, TagString) ::minEngine::NativeGameplayTag TagName(TagString)

/// Define a file-local native gameplay tag (UE_DEFINE_GAMEPLAY_TAG_STATIC analogue). Use in .cpp only.
#define ME_DEFINE_GAMEPLAY_TAG_STATIC(TagName, TagString) static ::minEngine::NativeGameplayTag TagName(TagString)

ME_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Channel_Default);
