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

        Add({"scene", "move_component", "edit", "scene.move_component", true, true, ""});
        Add({"scene", "instantiate_prefab", "edit", "scene.instantiate_prefab", true, true, ""});
        Add({"scene", "select_object", "control", "scene.select_object", true, false, "Select only; does not focus the session"});

        Add({"reflect", "list_classes", "query", "reflect.list_classes", false, false, ""});
        Add({"reflect", "list_properties", "query", "reflect.list_properties", false, false, ""});

        Add({"asset", "list", "query", "asset.list", false, false, ""});
        Add({"asset", "create", "asset", "asset.create", false, false, "Not a Scene undo unit"});
        Add({"asset", "delete", "asset", "asset.delete", false, false, "Not a Scene undo unit"});
        Add({"asset", "open", "control", "asset.open", false, false, "Open and focus; no file dialog"});

        Add({"editor", "activate_session", "control", "editor.activate_session", true, false, ""});
        Add({"editor", "reveal_asset", "control", "editor.reveal_asset", false, false, ""});
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
