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
        Add({"editor", "get_log", "query", "editor.get_log", false, false, "Recent console records and session log path"});
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
        Add({"asset", "list_import_products", "query", "asset.list_import_products", false, false, "Discover products/options for a source"});
        Add({"asset", "create", "asset", "asset.create", false, false,
            "Create asset; DataAsset may pass runtimeClassName"});
        Add({"asset", "import", "asset", "asset.import", false, false, "Unified import facade; products[] + options"});
        Add({"asset", "delete", "asset", "asset.delete", false, false, "Not a Scene undo unit"});
        Add({"asset", "open", "control", "asset.open", false, false, "Open and focus; no file dialog"});
        Add({"asset", "create_folder", "asset", "asset.create_folder", false, false, "Empty content folder"});
        Add({"asset", "move", "asset", "asset.move", false, false, "Move asset; Guid stable"});
        Add({"asset", "rename", "asset", "asset.rename", false, false, "Rename asset file stem+ext"});
        Add({"asset", "rename_folder", "asset", "asset.rename_folder", false, false, "Rename empty folder"});
        Add({"asset", "delete_folder", "asset", "asset.delete_folder", false, false, "Delete empty folder"});
        Add({"asset", "batch_rename", "asset", "asset.batch_rename", false, false, "Batch rename Foo/Foo_1..."});

        Add({
            "prefab",
            "create_from_game_object",
            "asset",
            "prefab.create_from_game_object",
            true,
            false,
            "Serialize a scene GameObject subtree to a Prefab asset; scene stays untouched"});
        Add({"prefab", "open", "control", "prefab.open", false, false, "Open a Prefab Stage session"});
        Add({
            "prefab",
            "save",
            "asset",
            "prefab.save",
            false,
            false,
            "Write the Prefab Stage tree back to its asset; asset Guid preserved"});
        Add({"prefab", "close", "control", "prefab.close", false, false, "Close a Prefab Stage session"});

        Add({"sprite", "create_from_texture", "asset", "sprite.create_from_texture", false, false, "Create Sprite from Texture2D"});
        Add({"sprite", "save", "asset", "sprite.save", false, false, "Save Sprite asset"});
        Add({"sprite_atlas", "create", "asset", "sprite_atlas.create", false, false, "Create empty SpriteAtlas"});
        Add({"sprite_atlas", "add_members", "asset", "sprite_atlas.add_members", false, false, "Add Sprite members and save"});
        Add({"sprite_atlas", "set_padding", "asset", "sprite_atlas.set_padding", false, false, "Set atlas padding/mode and save"});
        Add({"sprite_atlas", "set_sampler", "asset", "sprite_atlas.set_sampler", false, false, "Set atlas sampler filter/address and save"});
        Add({"sprite_atlas", "rebuild", "asset", "sprite_atlas.rebuild", false, false, "Rebuild atlas and save (needs RHI)"});
        Add({"sprite_atlas", "save", "asset", "sprite_atlas.save", false, false, "Save SpriteAtlas"});
        Add({"sprite_atlas", "create_from_selection", "asset", "sprite_atlas.create_from_selection", false, false, "Create atlas from Sprite paths"});

        Add({"editor", "activate_session", "control", "editor.activate_session", true, false, ""});
        Add({"editor", "reveal_asset", "control", "editor.reveal_asset", false, false, ""});

        Add({"material", "get_graph", "query", "material.get_graph", true, false, ""});
        Add({"material", "add_node", "edit", "material.add_node", true, true, ""});
        Add({"material", "remove_node", "edit", "material.remove_node", true, true, ""});
        Add({"material", "connect_pins", "edit", "material.connect_pins", true, true, ""});
        Add({"material", "disconnect_input", "edit", "material.disconnect_input", true, true, ""});
        Add({"material", "set_property", "edit", "material.set_property", true, true, ""});
        Add({"material", "save", "edit", "material.save", true, false, ""});
        Add({"material", "compile", "edit", "material.compile", true, false, "Not an undo unit"});

        Add({"animgraph", "get_graph", "query", "animgraph.get_graph", true, false, ""});
        Add({"animgraph", "add_state", "edit", "animgraph.add_state", true, true, ""});
        Add({"animgraph", "remove_state", "edit", "animgraph.remove_state", true, true, ""});
        Add({"animgraph", "rename_state", "edit", "animgraph.rename_state", true, true, ""});
        Add({"animgraph", "add_transition", "edit", "animgraph.add_transition", true, true, ""});
        Add({"animgraph", "add_any_state_transition", "edit", "animgraph.add_any_state_transition", true, true, ""});
        Add({"animgraph", "remove_transition", "edit", "animgraph.remove_transition", true, true, ""});
        Add({"animgraph", "remove_any_state_transition", "edit", "animgraph.remove_any_state_transition", true, true, ""});
        Add({"animgraph", "set_default_state", "edit", "animgraph.set_default_state", true, true, ""});
        Add({"animgraph", "set_property", "edit", "animgraph.set_property", true, true, ""});
        Add({"animgraph", "save", "edit", "animgraph.save", true, false, ""});
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
