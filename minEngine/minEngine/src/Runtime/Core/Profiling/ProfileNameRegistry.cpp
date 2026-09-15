#include "Runtime/Core/Profiling/ProfileNameRegistry.h"

#include <cstring>

namespace minEngine::Profile
{
    ProfileNameId ProfileNameRegistry::GetOrRegister(const char* staticLiteral)
    {
        if (staticLiteral == nullptr || staticLiteral[0] == '\0')
        {
            return kInvalidNameId;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);

        if (m_IdToText.empty())
        {
            m_IdToText.push_back(nullptr);
        }

        const auto byPtr = m_ByPointer.find(staticLiteral);
        if (byPtr != m_ByPointer.end())
        {
            return byPtr->second;
        }

        const auto byStr = m_ByString.find(staticLiteral);
        if (byStr != m_ByString.end())
        {
            m_ByPointer.emplace(staticLiteral, byStr->second);
            return byStr->second;
        }

        const ProfileNameId id = static_cast<ProfileNameId>(m_IdToText.size());
        m_IdToText.push_back(staticLiteral);
        m_ByPointer.emplace(staticLiteral, id);
        m_ByString.emplace(staticLiteral, id);
        return id;
    }

    const char* ProfileNameRegistry::GetText(ProfileNameId id) const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (id == kInvalidNameId || id >= m_IdToText.size())
        {
            return "";
        }
        const char* text = m_IdToText[id];
        return text != nullptr ? text : "";
    }

    size_t ProfileNameRegistry::GetCount() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_IdToText.empty() ? 0 : (m_IdToText.size() - 1);
    }
}
