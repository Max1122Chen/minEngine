#include "Services/EditorMaterialDebugCommands.h"

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandPayloadJson.h"
#include "DebugCommand/DebugCommandRegistry.h"
#include "DebugCommand/DebugCommandResult.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Material/MaterialGraphNodeRegistry.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class EditorMaterialDebugCommandsImpl
    {
    public:
        static IEditorContext* GetEditorContext(const DebugCommand::DebugCommandContext& context)
        {
            return static_cast<IEditorContext*>(context.EditorContextOpaque);
        }

        static DebugCommand::DebugCommandResult MissingEditor()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: this command is only available in the editor.");
            return builder.BuildError("editor only");
        }

        static DebugCommand::DebugCommandResult MissingMaterial()
        {
            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddLine(
                DebugCommand::DebugCommandOutputKind::Error,
                "Error: no open material session.");
            return builder.BuildError("no open material");
        }

        static bool ResolveMaterialEditor(
            const DebugCommand::DebugCommandContext& context,
            IEditorContext*& outEditor,
            MaterialEditor*& outMaterialEditor)
        {
            outEditor = GetEditorContext(context);
            outMaterialEditor = nullptr;
            if (outEditor == nullptr)
            {
                return false;
            }

            outMaterialEditor = GetMaterialEditor(outEditor);
            return outMaterialEditor != nullptr && outMaterialEditor->GetSession().HasOpenMaterial();
        }

        static bool EqualsIgnoreCase(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
            {
                return false;
            }

            for (size_t i = 0; i < left.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(left[i]))
                    != std::tolower(static_cast<unsigned char>(right[i])))
                {
                    return false;
                }
            }
            return true;
        }

        static bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix)
        {
            if (prefix.size() > text.size())
            {
                return false;
            }

            for (size_t i = 0; i < prefix.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(text[i]))
                    != std::tolower(static_cast<unsigned char>(prefix[i])))
                {
                    return false;
                }
            }
            return true;
        }

        static bool TryParseGuid(std::string_view text, GUID& outGuid)
        {
            unsigned int a = 0;
            unsigned int b = 0;
            unsigned int c = 0;
            unsigned int d = 0;
            unsigned long long e = 0;
            const std::string normalized(text);
            if (std::sscanf(
                    normalized.c_str(),
                    "%08x-%04x-%04x-%04x-%012llx",
                    &a,
                    &b,
                    &c,
                    &d,
                    &e)
                != 5)
            {
                return false;
            }

            outGuid.High = (static_cast<uint64_t>(a) << 32) | (static_cast<uint64_t>(b) << 16)
                | static_cast<uint64_t>(c);
            outGuid.Low = (static_cast<uint64_t>(d) << 48) | e;
            return true;
        }

        static bool TryParseIndex(std::string_view text, size_t& outIndex)
        {
            if (text.empty())
            {
                return false;
            }

            char* parseEnd = nullptr;
            const unsigned long value = std::strtoul(std::string(text).c_str(), &parseEnd, 10);
            if (parseEnd == nullptr || *parseEnd != '\0')
            {
                return false;
            }

            outIndex = static_cast<size_t>(value);
            return true;
        }

        static bool TryParseInt32(std::string_view text, int32_t& outValue)
        {
            if (text.empty())
            {
                return false;
            }

            char* parseEnd = nullptr;
            const long value = std::strtol(std::string(text).c_str(), &parseEnd, 10);
            if (parseEnd == nullptr || *parseEnd != '\0')
            {
                return false;
            }

            outValue = static_cast<int32_t>(value);
            return true;
        }

        static bool TryParseFloat(std::string_view text, float& outValue)
        {
            if (text.empty())
            {
                return false;
            }

            char* parseEnd = nullptr;
            const float value = std::strtof(std::string(text).c_str(), &parseEnd);
            if (parseEnd == nullptr || *parseEnd != '\0')
            {
                return false;
            }

            outValue = value;
            return true;
        }

        static MaterialEdGraph* GetGraph(MaterialEditor& materialEditor)
        {
            if (!materialEditor.GetSession().HasOpenMaterial()
                || !materialEditor.GetSession().MaterialAsset->m_Graph)
            {
                return nullptr;
            }

            return materialEditor.GetSession().MaterialAsset->m_Graph.get();
        }

        static MaterialEdGraphNode* ResolveNode(
            MaterialEdGraph& graph,
            std::string_view token,
            std::string* outError)
        {
            size_t index = 0;
            if (TryParseIndex(token, index))
            {
                if (index >= graph.m_Nodes.size() || !graph.m_Nodes[index])
                {
                    if (outError != nullptr)
                    {
                        *outError = "node index out of range";
                    }
                    return nullptr;
                }
                return graph.m_Nodes[index].get();
            }

            GUID guid;
            if (TryParseGuid(token, guid))
            {
                for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
                {
                    if (!node || node->GetNodeDef() == nullptr)
                    {
                        continue;
                    }
                    if (node->GetNodeDef()->GetGuid() == guid)
                    {
                        return node.get();
                    }
                }

                if (outError != nullptr)
                {
                    *outError = "node guid not found";
                }
                return nullptr;
            }

            MaterialEdGraphNode* matched = nullptr;
            size_t matchCount = 0;
            for (const std::shared_ptr<MaterialEdGraphNode>& node : graph.m_Nodes)
            {
                if (!node || node->GetNodeDef() == nullptr)
                {
                    continue;
                }

                const std::string guidText = node->GetNodeDef()->GetGuid().ToString();
                if (StartsWithIgnoreCase(guidText, token))
                {
                    matched = node.get();
                    ++matchCount;
                }
            }

            if (matchCount == 1)
            {
                return matched;
            }

            if (outError != nullptr)
            {
                *outError = matchCount > 1 ? "ambiguous node token" : "node not found";
            }
            return nullptr;
        }

        static bool ResolveNodeDefClassName(std::string_view token, std::string& outClassName)
        {
            MaterialGraphNodeRegistry::EnsureRegistered();
            const std::vector<MaterialGraphNodeRegistryEntry>& entries =
                MaterialGraphNodeRegistry::GetCreatableNodes();

            for (const MaterialGraphNodeRegistryEntry& entry : entries)
            {
                if (entry.NodeDefClass == nullptr)
                {
                    continue;
                }

                const std::string className = entry.NodeDefClass->GetName();
                if (EqualsIgnoreCase(className, token) || EqualsIgnoreCase(entry.DisplayName, token))
                {
                    outClassName = className;
                    return true;
                }
            }

            return false;
        }

        static bool TryParseShadingModel(std::string_view text, MaterialShadingModel& outModel)
        {
            if (EqualsIgnoreCase(text, "Unlit") || text == "0")
            {
                outModel = MaterialShadingModel::Unlit;
                return true;
            }
            if (EqualsIgnoreCase(text, "BlinnPhong") || text == "1")
            {
                outModel = MaterialShadingModel::BlinnPhong;
                return true;
            }
            if (EqualsIgnoreCase(text, "PBR") || text == "2")
            {
                outModel = MaterialShadingModel::PBR;
                return true;
            }
            return false;
        }

        static bool TryParseBlendMode(std::string_view text, MaterialBlendMode& outMode)
        {
            if (EqualsIgnoreCase(text, "Opaque") || text == "0")
            {
                outMode = MaterialBlendMode::Opaque;
                return true;
            }
            if (EqualsIgnoreCase(text, "Masked") || text == "1")
            {
                outMode = MaterialBlendMode::Masked;
                return true;
            }
            if (EqualsIgnoreCase(text, "Translucent") || text == "2")
            {
                outMode = MaterialBlendMode::Translucent;
                return true;
            }
            return false;
        }

        static const char* ShadingModelName(MaterialShadingModel model)
        {
            switch (model)
            {
            case MaterialShadingModel::Unlit:
                return "Unlit";
            case MaterialShadingModel::BlinnPhong:
                return "BlinnPhong";
            case MaterialShadingModel::PBR:
                return "PBR";
            }
            return "Unknown";
        }

        static const char* BlendModeName(MaterialBlendMode mode)
        {
            switch (mode)
            {
            case MaterialBlendMode::Opaque:
                return "Opaque";
            case MaterialBlendMode::Masked:
                return "Masked";
            case MaterialBlendMode::Translucent:
                return "Translucent";
            }
            return "Unknown";
        }

        static DebugCommand::DebugCommandResult ExecuteListNodeTypes(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            (void)context;
            (void)args;

            MaterialGraphNodeRegistry::EnsureRegistered();
            DebugCommand::DebugCommandOutputBuilder builder;
            std::string payloadItems;
            size_t count = 0;
            for (const MaterialGraphNodeRegistryEntry& entry : MaterialGraphNodeRegistry::GetCreatableNodes())
            {
                if (entry.NodeDefClass == nullptr)
                {
                    continue;
                }

                ++count;
                const std::string className = entry.NodeDefClass->GetName();
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, entry.DisplayName);
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, className);
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"display\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(entry.DisplayName);
                payloadItems += ",\"class\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(className);
                payloadItems += '}';
            }

            std::string payload = "{\"op\":\"mat_list_node_types\",\"count\":";
            payload += std::to_string(count);
            payload += ",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteListNodes(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            (void)args;

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }

            MaterialEdGraph* graph = GetGraph(*materialEditor);
            if (graph == nullptr)
            {
                return MissingMaterial();
            }

            DebugCommand::DebugCommandOutputBuilder builder;
            std::string payloadItems;
            size_t count = 0;
            for (size_t i = 0; i < graph->m_Nodes.size(); ++i)
            {
                const std::shared_ptr<MaterialEdGraphNode>& node = graph->m_Nodes[i];
                if (!node)
                {
                    continue;
                }

                ++count;
                MaterialGraphNodeDef* nodeDef = node->GetNodeDef();
                const char* displayName = nodeDef != nullptr
                    ? MaterialGraphNodeRegistry::GetDisplayName(nodeDef)
                    : "(null)";
                const std::string className =
                    (nodeDef != nullptr && nodeDef->GetClass() != nullptr) ? nodeDef->GetClass()->GetName()
                                                                           : "";
                const std::string guidText =
                    nodeDef != nullptr ? nodeDef->GetGuid().ToString() : std::string();

                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, std::to_string(i));
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemName, displayName);
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "  ");
                builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, guidText);
                builder.NewLine();

                if (!payloadItems.empty())
                {
                    payloadItems += ',';
                }
                payloadItems += "{\"index\":";
                payloadItems += std::to_string(i);
                payloadItems += ",\"display\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(displayName);
                payloadItems += ",\"class\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(className);
                payloadItems += ",\"guid\":";
                payloadItems += DebugCommand::DebugCommandPayloadJson::Quote(guidText);
                payloadItems += ",\"x\":";
                payloadItems += std::to_string(node->m_EditorPosX);
                payloadItems += ",\"y\":";
                payloadItems += std::to_string(node->m_EditorPosY);
                payloadItems += '}';
            }

            const Material& material = *materialEditor->GetSession().MaterialAsset;
            std::string payload = "{\"op\":\"mat_list_nodes\",\"count\":";
            payload += std::to_string(count);
            payload += ",\"shading\":";
            payload += DebugCommand::DebugCommandPayloadJson::Quote(ShadingModelName(material.m_ShadingModel));
            payload += ",\"blend\":";
            payload += DebugCommand::DebugCommandPayloadJson::Quote(BlendModeName(material.m_BlendMode));
            payload += ",\"items\":[";
            payload += payloadItems;
            payload += "]}";
            builder.SetPayloadJson(std::move(payload));
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteAddNode(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_add_node requires <NodeType> [x y].");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }

            std::string className;
            if (!ResolveNodeDefClassName(args[0], className))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: unknown node type '" + args[0] + "'. Use mat_list_node_types.");
                return builder.BuildError("unknown node type");
            }

            float posX = 0.0f;
            float posY = 0.0f;
            if (args.size() >= 3)
            {
                if (!TryParseFloat(args[1], posX) || !TryParseFloat(args[2], posY))
                {
                    DebugCommand::DebugCommandOutputBuilder builder;
                    builder.AddLine(
                        DebugCommand::DebugCommandOutputKind::Error,
                        "Error: invalid position.");
                    return builder.BuildError("invalid position");
                }
            }
            else if (args.size() == 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_add_node position requires both x and y.");
                return builder.BuildError("missing y");
            }

            materialEditor->SubmitAddNode(*editorContext, className, posX, posY);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, className);
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, " @ ");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(posX) + "," + std::to_string(posY));
            builder.NewLine();
            return builder.BuildOk("added");
        }

        static DebugCommand::DebugCommandResult ExecuteRemoveNode(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_remove_node requires <index|guid>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }

            MaterialEdGraph* graph = GetGraph(*materialEditor);
            if (graph == nullptr)
            {
                return MissingMaterial();
            }

            std::string resolveError;
            MaterialEdGraphNode* node = ResolveNode(*graph, args[0], &resolveError);
            if (node == nullptr || node->GetNodeDef() == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + resolveError + ".");
                return builder.BuildError(resolveError);
            }

            const GUID nodeGuid = node->GetNodeDef()->GetGuid();
            materialEditor->SubmitRemoveNode(*editorContext, nodeGuid);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ListItemMeta,
                nodeGuid.ToString());
            builder.NewLine();
            return builder.BuildOk("removed");
        }

        static DebugCommand::DebugCommandResult ExecuteConnect(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 4)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_connect requires <from> <fromOut> <to> <toIn>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }

            MaterialEdGraph* graph = GetGraph(*materialEditor);
            if (graph == nullptr)
            {
                return MissingMaterial();
            }

            int32_t fromOut = 0;
            int32_t toIn = 0;
            if (!TryParseInt32(args[1], fromOut) || !TryParseInt32(args[3], toIn))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid pin index.");
                return builder.BuildError("invalid pin index");
            }

            std::string fromError;
            std::string toError;
            MaterialEdGraphNode* fromNode = ResolveNode(*graph, args[0], &fromError);
            MaterialEdGraphNode* toNode = ResolveNode(*graph, args[2], &toError);
            if (fromNode == nullptr || fromNode->GetNodeDef() == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: from node " + fromError + ".");
                return builder.BuildError(fromError);
            }
            if (toNode == nullptr || toNode->GetNodeDef() == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: to node " + toError + ".");
                return builder.BuildError(toError);
            }

            materialEditor->SubmitConnectPins(
                *editorContext,
                fromNode->GetNodeDef()->GetGuid(),
                fromOut,
                toNode->GetNodeDef()->GetGuid(),
                toIn);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ListItemMeta,
                fromNode->GetNodeDef()->GetGuid().ToString());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "[");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(fromOut));
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "] -> ");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ListItemMeta,
                toNode->GetNodeDef()->GetGuid().ToString());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "[");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(toIn));
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "]");
            builder.NewLine();
            return builder.BuildOk("connected");
        }

        static DebugCommand::DebugCommandResult ExecuteDisconnect(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.size() < 2)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_disconnect requires <to> <toIn>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }

            MaterialEdGraph* graph = GetGraph(*materialEditor);
            if (graph == nullptr)
            {
                return MissingMaterial();
            }

            int32_t toIn = 0;
            if (!TryParseInt32(args[1], toIn))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid pin index.");
                return builder.BuildError("invalid pin index");
            }

            std::string resolveError;
            MaterialEdGraphNode* toNode = ResolveNode(*graph, args[0], &resolveError);
            if (toNode == nullptr || toNode->GetNodeDef() == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: " + resolveError + ".");
                return builder.BuildError(resolveError);
            }

            MaterialGraphNodeDefInput* input = toNode->GetNodeDef()->GetInput(toIn);
            if (input == nullptr || !input->IsConnected() || input->NodeDef == nullptr)
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: input pin is not connected.");
                return builder.BuildError("not connected");
            }

            const GUID toGuid = toNode->GetNodeDef()->GetGuid();
            const GUID fromGuid = input->NodeDef->GetGuid();
            const int32_t fromOut = input->OutputIndex;
            materialEditor->SubmitDisconnectInput(*editorContext, toGuid, toIn, fromGuid, fromOut);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ListItemMeta, toGuid.ToString());
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "[");
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                std::to_string(toIn));
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::Muted, "] disconnected");
            builder.NewLine();
            return builder.BuildOk("disconnected");
        }

        static DebugCommand::DebugCommandResult ExecuteSetShading(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_set_shading requires <Unlit|BlinnPhong|PBR>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }
            (void)editorContext;

            MaterialShadingModel model = MaterialShadingModel::Unlit;
            if (!TryParseShadingModel(args[0], model))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid shading model.");
                return builder.BuildError("invalid shading");
            }

            materialEditor->SetShadingModel(model);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(
                DebugCommand::DebugCommandOutputKind::ValueLiteral,
                ShadingModelName(model));
            builder.NewLine();
            return builder.BuildOk("ok");
        }

        static DebugCommand::DebugCommandResult ExecuteSetBlend(
            const DebugCommand::DebugCommandContext& context,
            const std::vector<std::string>& args)
        {
            if (args.empty())
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: mat_set_blend requires <Opaque|Masked|Translucent>.");
                return builder.BuildError("missing arguments");
            }

            IEditorContext* editorContext = nullptr;
            MaterialEditor* materialEditor = nullptr;
            if (!ResolveMaterialEditor(context, editorContext, materialEditor))
            {
                return editorContext == nullptr ? MissingEditor() : MissingMaterial();
            }
            (void)editorContext;

            MaterialBlendMode mode = MaterialBlendMode::Opaque;
            if (!TryParseBlendMode(args[0], mode))
            {
                DebugCommand::DebugCommandOutputBuilder builder;
                builder.AddLine(
                    DebugCommand::DebugCommandOutputKind::Error,
                    "Error: invalid blend mode.");
                return builder.BuildError("invalid blend");
            }

            materialEditor->SetBlendMode(mode);

            DebugCommand::DebugCommandOutputBuilder builder;
            builder.AddSegment(DebugCommand::DebugCommandOutputKind::ValueLiteral, BlendModeName(mode));
            builder.NewLine();
            return builder.BuildOk("ok");
        }
    };

    void EditorMaterialDebugCommands::Register()
    {
        DebugCommand::DebugCommandRegistry& registry = DebugCommand::DebugCommandRegistry::Get();

        DebugCommand::DebugCommandDescriptor listTypes;
        listTypes.Id = "mat_list_node_types";
        listTypes.Domain = "Material";
        listTypes.DisplayName = "mat_list_node_types";
        listTypes.Description = "List creatable material graph node types";
        listTypes.Scope = DebugCommand::DebugCommandScope::Editor;
        listTypes.Execute = EditorMaterialDebugCommandsImpl::ExecuteListNodeTypes;
        registry.Register(std::move(listTypes));

        DebugCommand::DebugCommandDescriptor listNodes;
        listNodes.Id = "mat_list_nodes";
        listNodes.Domain = "Material";
        listNodes.DisplayName = "mat_list_nodes";
        listNodes.Description = "List nodes in the open material graph";
        listNodes.Scope = DebugCommand::DebugCommandScope::Editor;
        listNodes.Execute = EditorMaterialDebugCommandsImpl::ExecuteListNodes;
        registry.Register(std::move(listNodes));

        DebugCommand::DebugCommandDescriptor addNode;
        addNode.Id = "mat_add_node";
        addNode.Domain = "Material";
        addNode.DisplayName = "mat_add_node";
        addNode.Description = "Add a material graph node (SubmitAddNode)";
        addNode.Scope = DebugCommand::DebugCommandScope::Editor;
        addNode.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "NodeType",
                DebugCommand::DebugCommandArgType::String,
                true,
                "DisplayName or class"},
            DebugCommand::DebugCommandArgDescriptor{
                "X",
                DebugCommand::DebugCommandArgType::Float,
                false,
                "Editor X"},
            DebugCommand::DebugCommandArgDescriptor{
                "Y",
                DebugCommand::DebugCommandArgType::Float,
                false,
                "Editor Y"},
        };
        addNode.Execute = EditorMaterialDebugCommandsImpl::ExecuteAddNode;
        registry.Register(std::move(addNode));

        DebugCommand::DebugCommandDescriptor removeNode;
        removeNode.Id = "mat_remove_node";
        removeNode.Domain = "Material";
        removeNode.DisplayName = "mat_remove_node";
        removeNode.Description = "Remove a material graph node by index or guid";
        removeNode.Scope = DebugCommand::DebugCommandScope::Editor;
        removeNode.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Node",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Index or guid"},
        };
        removeNode.Execute = EditorMaterialDebugCommandsImpl::ExecuteRemoveNode;
        registry.Register(std::move(removeNode));

        DebugCommand::DebugCommandDescriptor connect;
        connect.Id = "mat_connect";
        connect.Domain = "Material";
        connect.DisplayName = "mat_connect";
        connect.Description = "Connect material pins (SubmitConnectPins)";
        connect.Scope = DebugCommand::DebugCommandScope::Editor;
        connect.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "From",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Index or guid"},
            DebugCommand::DebugCommandArgDescriptor{
                "FromOut",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "Output pin"},
            DebugCommand::DebugCommandArgDescriptor{
                "To",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Index or guid"},
            DebugCommand::DebugCommandArgDescriptor{
                "ToIn",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "Input pin"},
        };
        connect.Execute = EditorMaterialDebugCommandsImpl::ExecuteConnect;
        registry.Register(std::move(connect));

        DebugCommand::DebugCommandDescriptor disconnect;
        disconnect.Id = "mat_disconnect";
        disconnect.Domain = "Material";
        disconnect.DisplayName = "mat_disconnect";
        disconnect.Description = "Disconnect a material input pin";
        disconnect.Scope = DebugCommand::DebugCommandScope::Editor;
        disconnect.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "To",
                DebugCommand::DebugCommandArgType::String,
                true,
                "Index or guid"},
            DebugCommand::DebugCommandArgDescriptor{
                "ToIn",
                DebugCommand::DebugCommandArgType::Int,
                true,
                "Input pin"},
        };
        disconnect.Execute = EditorMaterialDebugCommandsImpl::ExecuteDisconnect;
        registry.Register(std::move(disconnect));

        DebugCommand::DebugCommandDescriptor setShading;
        setShading.Id = "mat_set_shading";
        setShading.Domain = "Material";
        setShading.DisplayName = "mat_set_shading";
        setShading.Description = "Set material shading model (stacked)";
        setShading.Scope = DebugCommand::DebugCommandScope::Editor;
        setShading.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Shading",
                DebugCommand::DebugCommandArgType::Enum,
                true,
                "Unlit|BlinnPhong|PBR"},
        };
        setShading.Execute = EditorMaterialDebugCommandsImpl::ExecuteSetShading;
        registry.Register(std::move(setShading));

        DebugCommand::DebugCommandDescriptor setBlend;
        setBlend.Id = "mat_set_blend";
        setBlend.Domain = "Material";
        setBlend.DisplayName = "mat_set_blend";
        setBlend.Description = "Set material blend mode (stacked)";
        setBlend.Scope = DebugCommand::DebugCommandScope::Editor;
        setBlend.Args = {
            DebugCommand::DebugCommandArgDescriptor{
                "Blend",
                DebugCommand::DebugCommandArgType::Enum,
                true,
                "Opaque|Masked|Translucent"},
        };
        setBlend.Execute = EditorMaterialDebugCommandsImpl::ExecuteSetBlend;
        registry.Register(std::move(setBlend));
    }
}
