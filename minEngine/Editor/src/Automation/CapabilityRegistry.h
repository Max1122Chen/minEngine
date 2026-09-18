#pragma once

#include "Runtime/Core/Serialization/Json.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    struct AutomationCapability
    {
        std::string Domain;
        std::string Name;
        std::string Kind; // query | edit | verify
        std::string Op;
        bool SessionRequired = false;
        bool Undo = false;
        std::string Description;
    };

    class CapabilityRegistry
    {
    public:
        CapabilityRegistry();

        void RegisterBuiltinSceneMvp();

        const AutomationCapability* FindByOp(std::string_view op) const;
        Json BuildCapabilitiesDocument(const std::string& editorVersion) const;

        const std::vector<AutomationCapability>& GetAll() const { return m_Capabilities; }

    private:
        void Add(AutomationCapability capability);

        std::vector<AutomationCapability> m_Capabilities;
        std::unordered_map<std::string, size_t> m_OpIndex;
    };
}
