#include "Automation/CapabilityRegistry.h"

#include "Runtime/Core/ProductBranding.h"

namespace minEngine
{
    CapabilityRegistry::CapabilityRegistry()
    {
        RegisterBuiltinSceneMvp();
    }

    void CapabilityRegistry::Add(AutomationCapability capability)
    {
        const std::string op = capability.Op;
        m_OpIndex[op] = m_Capabilities.size();
        m_Capabilities.push_back(std::move(capability));
    }

    void CapabilityRegistry::RegisterBuiltinSceneMvp()
    {
        m_Capabilities.clear();
        m_OpIndex.clear();

        Add({"editor", "ping", "query", "editor.ping", false, false, "Internal connectivity check"});
        Add({"editor", "get_state", "query", "editor.get_state", false, false, "Project path and open sessions"});
        Add({"editor", "get_capabilities", "query", "editor.get_capabilities", false, false, ""});

        Add({"scene", "get_hierarchy", "query", "scene.get_hierarchy", true, false, ""});
        Add({"scene", "get_object", "query", "scene.get_object", true, false, ""});
        Add({"scene", "get_property", "query", "scene.get_property", true, false, ""});
        Add({"scene", "create_game_object", "edit", "scene.create_game_object", true, true, ""});
        Add({"scene", "delete_game_object", "edit", "scene.delete_game_object", true, true, ""});
        Add({"scene", "add_component", "edit", "scene.add_component", true, true, ""});
        Add({"scene", "remove_component", "edit", "scene.remove_component", true, true, ""});
        Add({"scene", "set_property", "edit", "scene.set_property", true, true, ""});
        Add({"scene", "reparent", "edit", "scene.reparent", true, true, ""});
        Add({"scene", "save", "edit", "scene.save", true, false, ""});
        Add({
            "scene",
            "validate",
            "verify",
            "scene.validate",
            true,
            false,
            "P0 stub: session/object/path resolvability only"});
    }

    const AutomationCapability* CapabilityRegistry::FindByOp(std::string_view op) const
    {
        const auto it = m_OpIndex.find(std::string(op));
        if (it == m_OpIndex.end())
        {
            return nullptr;
        }
        return &m_Capabilities[it->second];
    }

    Json CapabilityRegistry::BuildCapabilitiesDocument(const std::string& editorVersion) const
    {
        Json domains = Json::array();
        std::unordered_map<std::string, size_t> domainIndex;

        for (const AutomationCapability& capability : m_Capabilities)
        {
            auto domainIt = domainIndex.find(capability.Domain);
            if (domainIt == domainIndex.end())
            {
                domainIndex[capability.Domain] = domains.size();
                domains.push_back(Json{
                    {"name", capability.Domain},
                    {"actions", Json::array()}});
                domainIt = domainIndex.find(capability.Domain);
            }

            Json action = {
                {"name", capability.Name},
                {"kind", capability.Kind},
                {"op", capability.Op},
                {"sessionRequired", capability.SessionRequired}};
            if (!capability.Description.empty())
            {
                action["description"] = capability.Description;
            }
            if (capability.Kind == "edit")
            {
                action["undo"] = capability.Undo;
            }
            domains[domainIt->second]["actions"].push_back(std::move(action));
        }

        return Json{
            {"protocolVersion", 1},
            {"editorProduct", GetProductDisplayName()},
            {"editorVersion", editorVersion},
            {"domains", std::move(domains)}};
    }
}
