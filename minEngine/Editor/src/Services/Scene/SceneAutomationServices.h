#pragma once

#include "Services/Scene/ISceneAutomationServices.h"

#include "Runtime/Core/GUID/GUID.h"

#include <memory>
#include <string_view>
#include <unordered_set>

namespace minEngine
{
    class EditorDocumentSession;
    class GameObject;
    class MEObject;
    class Scene;
    class SceneEditor;
    class Component;

    class SceneAutomationServices : public ISceneQueryService,
                                    public ISceneEditService,
                                    public ISceneVerifyService
    {
    public:
        SceneServiceResult GetHierarchy(
            IEditorContext& context,
            std::string_view sessionId,
            int maxDepth) override;

        SceneServiceResult GetObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId) override;

        SceneServiceResult GetProperty(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath) override;

        SceneServiceResult CreateGameObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view name,
            std::string_view parentObjectId) override;

        SceneServiceResult DeleteGameObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId) override;

        SceneServiceResult AddComponent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view className) override;

        SceneServiceResult RemoveComponent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view componentObjectId) override;

        SceneServiceResult SetProperty(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath,
            const Json& value) override;

        SceneServiceResult Reparent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view newParentObjectId) override;

        SceneServiceResult SaveDocument(IEditorContext& context, std::string_view sessionId) override;

        SceneServiceResult Validate(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath) override;

    private:
        SceneServiceResult MakeError(
            std::string code,
            std::string message,
            Json extra = Json::object()) const;

        static EditorDocumentSession* ResolveSession(
            IEditorContext& context,
            std::string_view sessionId,
            SceneServiceResult& outError);

        static SceneEditor* ResolveSceneEditor(IEditorContext& context, SceneServiceResult& outError);

        static Scene* ResolveDocumentScene(SceneEditor& sceneEditor, SceneServiceResult& outError);

        static std::shared_ptr<MEObject> ResolveOwner(
            std::string_view objectId,
            SceneServiceResult& outError,
            GUID& outGuid);

        static GameObject* ResolveGameObject(
            Scene& scene,
            std::string_view objectId,
            SceneServiceResult& outError);

        static Component* ResolveComponent(
            Scene& scene,
            std::string_view componentObjectId,
            GameObject*& outOwner,
            SceneServiceResult& outError);

        static void ActivateSession(IEditorContext& context, EditorDocumentSession& session);

        static Json BuildHierarchyNode(GameObject& gameObject, int depthRemaining);

        static std::unordered_set<std::string> CollectGameObjectGuidSet(Scene& scene);

        /** Accepts full reflection name or unambiguous short suffix (e.g. StaticMeshComponent). */
        static bool TryResolveComponentTypeName(
            SceneEditor& sceneEditor,
            std::string_view className,
            std::string& outResolvedTypeName,
            SceneServiceResult& outError);
    };
}
