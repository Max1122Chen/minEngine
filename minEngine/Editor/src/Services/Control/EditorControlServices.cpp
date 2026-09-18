#include "Services/Control/EditorControlServices.h"

#include "Automation/AutomationProtocol.h"
#include "Services/AssetWorkflowModule.h"
#include "Services/Scene/SceneEditorServices.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/Document/EditorDocumentId.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    EditorControlResult EditorControlServices::ActivateSession(
        IEditorContext& context,
        std::string_view sessionId)
    {
        EditorControlResult result;
        uint64_t documentId = 0;
        if (!AutomationProtocol::TryParseSessionId(sessionId, documentId) || documentId == 0)
        {
            result.Message = "sessionId is invalid.";
            return result;
        }

        EditorDocumentId id;
        id.Value = documentId;
        if (!context.GetDocumentHost().Activate(id))
        {
            result.Message = "Failed to activate session.";
            return result;
        }
        result.Ok = true;
        result.SessionId = AutomationProtocol::FormatSessionId(documentId);
        return result;
    }

    EditorControlResult EditorControlServices::OpenAsset(
        IEditorContext& context,
        std::string_view assetPath)
    {
        EditorControlResult result;
        if (assetPath.empty())
        {
            result.Message = "path is required.";
            return result;
        }
        if (!AssetManager::HasInstance())
        {
            result.Message = "AssetManager is not available.";
            return result;
        }

        const std::string pathStorage(assetPath);
        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(pathStorage);
        if (meta == nullptr)
        {
            result.Message = "Asset not found.";
            return result;
        }

        EditorDocumentHost& host = context.GetDocumentHost();
        if (!host.OpenOrFocus(*meta))
        {
            result.Message = "Failed to open asset. The current document of that type may be dirty.";
            return result;
        }

        EditorDocumentSession* session = host.GetActiveSession();
        if (session == nullptr)
        {
            result.Message = "Asset opened but session was not found.";
            return result;
        }

        result.Ok = true;
        result.SessionId = AutomationProtocol::FormatSessionId(session->GetId().Value);
        return result;
    }

    EditorControlResult EditorControlServices::RevealAsset(
        IEditorContext& context,
        std::string_view assetPath)
    {
        EditorControlResult result;
        if (!context.GetAssetWorkflow().RevealAssetInContentBrowser(assetPath))
        {
            result.Message = "Failed to reveal asset.";
            return result;
        }
        result.Ok = true;
        return result;
    }

    EditorControlResult EditorControlServices::SelectObject(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& objectId)
    {
        EditorControlResult result;
        SceneEditor* sceneEditor = GetSceneEditor(&context);
        if (sceneEditor == nullptr)
        {
            result.Message = "SceneEditor is unavailable.";
            return result;
        }

        Scene* sessionScene = nullptr;
        const SceneEditorResult sceneResult =
            context.GetSceneEditorServices().GetScene(context, session, &sessionScene);
        if (!sceneResult.Ok || sessionScene == nullptr)
        {
            result.Message = sceneResult.Message.empty() ? "Session has no scene." : sceneResult.Message;
            return result;
        }

        if (sceneEditor->GetActiveScene() != sessionScene)
        {
            result.Message = "Session is not the scene currently shown. Call activate_session first.";
            return result;
        }

        GameObject* gameObject = nullptr;
        const SceneEditorResult findResult =
            context.GetSceneEditorServices().FindGameObject(*sessionScene, objectId, &gameObject);
        if (!findResult.Ok || gameObject == nullptr)
        {
            result.Message = findResult.Message.empty() ? "Object not found." : findResult.Message;
            return result;
        }

        sceneEditor->SelectGameObject(gameObject->GetID());
        result.Ok = true;
        result.SessionId = AutomationProtocol::FormatSessionId(session.GetId().Value);
        return result;
    }
}
