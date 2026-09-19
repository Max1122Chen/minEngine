#include "Services/Material/MaterialEditorServices.h"

#include "Commands/Material/EditorAddMaterialNodeCommand.h"
#include "Commands/Material/EditorConnectMaterialPinsCommand.h"
#include "Commands/Material/EditorDisconnectMaterialPinCommand.h"
#include "Commands/Material/EditorRemoveMaterialNodeCommand.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "SubEditor/Material/MaterialEditor.h"

#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialEdGraphNode.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <unordered_set>
#include <utility>

namespace minEngine
{
    class MaterialDocumentBinding
    {
    public:
        MaterialDocumentBinding() = default;
        MaterialDocumentBinding(const MaterialDocumentBinding&) = delete;
        MaterialDocumentBinding& operator=(const MaterialDocumentBinding&) = delete;

        ~MaterialDocumentBinding()
        {
            if (m_Editor == nullptr || !m_Switched)
            {
                return;
            }

            m_Editor->StashActiveSession();
            if (!m_RestoreKey.empty())
            {
                m_Editor->ActivateStoredSession(m_RestoreKey);
            }
        }

        bool Bind(MaterialEditor& editor, const std::string& assetKey, GraphEditorResult& outError)
        {
            m_Editor = &editor;
            if (editor.GetSession().HasOpenMaterial() && editor.GetSession().AssetPath == assetKey)
            {
                return true;
            }

            const std::string previous =
                editor.GetSession().HasOpenMaterial() ? editor.GetSession().AssetPath : std::string();
            if (!editor.ActivateStoredSession(assetKey))
            {
                m_Editor = nullptr;
                outError.Ok = false;
                outError.Code = GraphEditorError::NotFound;
                outError.Message = "Material session is not loaded.";
                return false;
            }

            m_Switched = true;
            m_RestoreKey = previous;
            return true;
        }

    private:
        MaterialEditor* m_Editor = nullptr;
        std::string m_RestoreKey;
        bool m_Switched = false;
    };

    GraphEditorResult MaterialEditorServices::OkResult()
    {
        GraphEditorResult result;
        result.Ok = true;
        return result;
    }

    GraphEditorResult MaterialEditorServices::Fail(GraphEditorError code, std::string message)
    {
        GraphEditorResult result;
        result.Ok = false;
        result.Code = code;
        result.Message = std::move(message);
        return result;
    }

    GraphEditorResult MaterialEditorServices::EnsureMaterialSession(const EditorDocumentSession& session)
    {
        if (session.GetTypeId() != "Material")
        {
            return Fail(GraphEditorError::InvalidArgument, "Session is not a Material document.");
        }
        if (session.GetAssetKey().empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "Material session has no asset key.");
        }
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::GetMaterial(
        IEditorContext& context,
        EditorDocumentSession& session,
        Material** outMaterial) const
    {
        if (outMaterial == nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "outMaterial is null.");
        }
        *outMaterial = nullptr;

        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        Material* material = editor->FindOpenMaterial(session.GetAssetKey());
        if (material == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Material is not open for this session.");
        }

        *outMaterial = material;
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::AddNode(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view nodeDefClassName,
        float editorPosX,
        float editorPosY,
        GUID* outNodeDefGuid)
    {
        if (outNodeDefGuid != nullptr)
        {
            *outNodeDefGuid = GUID::Zero();
        }

        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (nodeDefClassName.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "nodeDefClassName is empty.");
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        std::unordered_set<std::string> beforeGuids;
        Material* material = editor->GetSession().MaterialAsset.get();
        if (material == nullptr || material->m_Graph == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Material graph is not open.");
        }
        for (const std::shared_ptr<MaterialEdGraphNode>& node : material->m_Graph->m_Nodes)
        {
            if (node && node->GetNodeDef() != nullptr)
            {
                beforeGuids.insert(node->GetNodeDef()->GetGuid().ToString());
            }
        }

        session.GetCommandStack().Execute(std::make_unique<EditorAddMaterialNodeCommand>(
            *editor,
            std::string(nodeDefClassName),
            editorPosX,
            editorPosY));

        GUID created = GUID::Zero();
        bool found = false;
        for (const std::shared_ptr<MaterialEdGraphNode>& node : material->m_Graph->m_Nodes)
        {
            if (!node || node->GetNodeDef() == nullptr)
            {
                continue;
            }
            const std::string guidText = node->GetNodeDef()->GetGuid().ToString();
            if (beforeGuids.find(guidText) == beforeGuids.end())
            {
                created = node->GetNodeDef()->GetGuid();
                found = true;
                break;
            }
        }

        if (!found)
        {
            return Fail(GraphEditorError::Internal, "Add node failed.");
        }

        if (outNodeDefGuid != nullptr)
        {
            *outNodeDefGuid = created;
        }
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::RemoveNode(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& nodeDefGuid)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        std::vector<uint8_t> snapshot;
        std::vector<MaterialNodeInboundLink> inboundLinks;
        if (!editor->TryCaptureRemoveNode(nodeDefGuid, snapshot, inboundLinks))
        {
            return Fail(GraphEditorError::NotFound, "Material node was not found or cannot be removed.");
        }

        session.GetCommandStack().Execute(std::make_unique<EditorRemoveMaterialNodeCommand>(
            *editor,
            nodeDefGuid.High,
            nodeDefGuid.Low,
            std::move(snapshot),
            std::move(inboundLinks)));
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::ConnectPins(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& fromNodeDefGuid,
        int32_t fromOutputIndex,
        const GUID& toNodeDefGuid,
        int32_t toInputIndex)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        MaterialEdGraphNode* toNode = editor->FindEdNodeByNodeDefGuid(toNodeDefGuid);
        if (toNode == nullptr || toNode->GetNodeDef() == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Target material node was not found.");
        }

        MaterialGraphNodeDefInput* input = toNode->GetNodeDef()->GetInput(toInputIndex);
        if (input == nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "Target input index is out of range.");
        }

        bool hadPrevious = false;
        GUID previousFromGuid;
        int32_t previousFromOutput = 0;
        if (input->IsConnected() && input->NodeDef != nullptr)
        {
            hadPrevious = true;
            previousFromGuid = input->NodeDef->GetGuid();
            previousFromOutput = input->OutputIndex;
        }

        session.GetCommandStack().Execute(std::make_unique<EditorConnectMaterialPinsCommand>(
            *editor,
            fromNodeDefGuid.High,
            fromNodeDefGuid.Low,
            fromOutputIndex,
            toNodeDefGuid.High,
            toNodeDefGuid.Low,
            toInputIndex,
            previousFromGuid.High,
            previousFromGuid.Low,
            previousFromOutput,
            hadPrevious));

        MaterialGraphNodeDefInput* afterInput = toNode->GetNodeDef()->GetInput(toInputIndex);
        const bool connected = afterInput != nullptr && afterInput->IsConnected()
            && afterInput->NodeDef != nullptr
            && afterInput->NodeDef->GetGuid() == fromNodeDefGuid
            && afterInput->OutputIndex == fromOutputIndex;
        if (!connected)
        {
            return Fail(GraphEditorError::InvalidArgument, "Connect pins failed.");
        }

        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::DisconnectInput(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& toNodeDefGuid,
        int32_t toInputIndex)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        MaterialEdGraphNode* toNode = editor->FindEdNodeByNodeDefGuid(toNodeDefGuid);
        if (toNode == nullptr || toNode->GetNodeDef() == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Target material node was not found.");
        }

        MaterialGraphNodeDefInput* input = toNode->GetNodeDef()->GetInput(toInputIndex);
        if (input == nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "Target input index is out of range.");
        }
        if (!input->IsConnected() || input->NodeDef == nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "Input is not connected.");
        }

        const GUID fromGuid = input->NodeDef->GetGuid();
        const int32_t fromOutput = input->OutputIndex;
        session.GetCommandStack().Execute(std::make_unique<EditorDisconnectMaterialPinCommand>(
            *editor,
            toNodeDefGuid.High,
            toNodeDefGuid.Low,
            toInputIndex,
            fromGuid.High,
            fromGuid.Low,
            fromOutput));
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::CommitPropertyBlobs(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& ownerGuid,
        std::string_view ownerClassName,
        std::string_view propertyPath,
        std::vector<uint8_t> beforeValue,
        std::vector<uint8_t> afterValue,
        bool applyOnFirstExecute,
        EditorSetObjectPropertySideEffects sideEffects)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (beforeValue == afterValue)
        {
            return OkResult();
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        session.GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *editor,
            ownerGuid,
            std::string(ownerClassName),
            std::string(propertyPath),
            std::move(beforeValue),
            std::move(afterValue),
            applyOnFirstExecute,
            std::move(sideEffects)));
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::SaveDocument(
        IEditorContext& context,
        EditorDocumentSession& session)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        if (!editor->SaveActiveMaterial())
        {
            return Fail(GraphEditorError::Internal, "Material save failed.");
        }
        return OkResult();
    }

    GraphEditorResult MaterialEditorServices::Compile(
        IEditorContext& context,
        EditorDocumentSession& session)
    {
        const GraphEditorResult sessionCheck = EnsureMaterialSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        MaterialEditor* editor = GetMaterialEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "MaterialEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        MaterialDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        editor->CompileActiveMaterial();
        Material* material = editor->GetSession().MaterialAsset.get();
        if (material == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Material is not open.");
        }

        std::string message;
        bool hasError = false;
        for (const MaterialCompileDiagnostic& diagnostic : material->m_LastCompileDiagnostics)
        {
            if (!message.empty())
            {
                message += "\n";
            }
            message += diagnostic.Message;
            if (diagnostic.Level == MaterialCompileDiagnostic::Error)
            {
                hasError = true;
            }
        }
        if (message.empty() && !material->GetShaderCompileLog().empty())
        {
            message = material->GetShaderCompileLog();
            hasError = !material->IsCompiledForDraw();
        }

        if (hasError)
        {
            return Fail(
                GraphEditorError::Internal,
                message.empty() ? std::string("Material compile failed.") : message);
        }

        GraphEditorResult result = OkResult();
        result.Message = std::move(message);
        return result;
    }
}
