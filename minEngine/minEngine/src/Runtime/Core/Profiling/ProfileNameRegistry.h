#pragma once

#include "Runtime/Core/Profiling/ProfileTypes.h"

#include "EngineAPI.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine::Profile
{
    class ProfileNameRegistry
    {
    public:
        MINENGINE_API ProfileNameId GetOrRegister(const char* staticLiteral);
        MINENGINE_API const char* GetText(ProfileNameId id) const;
        MINENGINE_API size_t GetCount() const;

    private:
        mutable std::mutex m_Mutex;
        std::unordered_map<const char*, ProfileNameId> m_ByPointer;
        std::unordered_map<std::string, ProfileNameId> m_ByString;
        std::vector<const char*> m_IdToText; // index 0 unused
    };
}
