#pragma once

#include "Services/Scene/SceneEditorResult.h"

#include "Runtime/Core/GUID/GUID.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace minEngine
{
    class Component;
    class EditorDocumentSession;
    class GameObject;
    class IEditorContext;
    class MEObject;
    class Prefab;
    class Scene;
    class SceneEditor;
    class Transform;
    class TypeQueryServices;

    /**
     * Native Scene editor capability surface (ED-F18).
     * No JSON, no protocol strings, no hierarchy assembly, no Active-session focus.
     */
    class SceneEditorServices
    {
    public:
        SceneEditorResult GetScene(
            IEditorContext& context,
            EditorDocumentSession& session,
            Scene** outScene) const;

        SceneEditorResult FindGameObject(
            Scene& scene,
            const GUID& id,
            GameObject** outObject) const;

        SceneEditorResult FindComponent(
            Scene& scene,
            const GUID& componentId,
            Component** outComponent,
            GameObject** outOwner) const;

        SceneEditorResult GetProperty(
            MEObject& owner,
            std::string_view propertyPath,
            void* outValue,
            size_t outValueSize) const;

        SceneEditorResult SetProperty(
            IEditorContext& context,
            EditorDocumentSession& session,
            MEObject& owner,
            std::string_view propertyPath,
            const void* value,
            size_t valueSize);

        /** Undo unit from precomputed serialization blobs (Inspector / Debug set transactions). */
        SceneEditorResult CommitObjectPropertyBlobs(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& ownerGuid,
            std::string_view ownerClassName,
            std::string_view propertyPath,
            std::vector<uint8_t> beforeValue,
            std::vector<uint8_t> afterValue,
            bool applyOnFirstExecute = true);

        SceneEditorResult CreateGameObject(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view name,
            GameObject* parent,
            GameObject** outCreated);

        SceneEditorResult DeleteGameObject(
            IEditorContext& context,
            EditorDocumentSession& session,
            GameObject& object);

        SceneEditorResult AddComponent(
            IEditorContext& context,
            EditorDocumentSession& session,
            GameObject& object,
            std::string_view typeName,
            Component** outAdded);

        SceneEditorResult RemoveComponent(
            IEditorContext& context,
            EditorDocumentSession& session,
            Component& component);

        SceneEditorResult Reparent(
            IEditorContext& context,
            EditorDocumentSession& session,
            GameObject& object,
            GameObject* newParent);

        SceneEditorResult MoveComponent(
            IEditorContext& context,
            EditorDocumentSession& session,
            GameObject& owner,
            Component& component,
            size_t newIndex);

        /**
         * Gizmo / human viewport: caller supplies before (mouse-down) and after (mouse-up).
         * Live preview is not a command; this is the single undo unit. Not SetProperty.
         */
        SceneEditorResult SetTransform(
            IEditorContext& context,
            EditorDocumentSession& session,
            GameObject& object,
            const Transform& before,
            const Transform& after);

        /** Command-only instantiate; asset pick / dialog stays in Editor control flow. */
        SceneEditorResult InstantiatePrefab(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::shared_ptr<Prefab> prefab,
            GameObject* attachParent);

        SceneEditorResult SaveDocument(IEditorContext& context, EditorDocumentSession& session);

        SceneEditorResult Validate(
            IEditorContext& context,
            EditorDocumentSession& session,
            MEObject* object,
            std::string_view propertyPath) const;

    private:
        static SceneEditorResult OkResult();
        static SceneEditorResult Fail(SceneEditorError code, std::string message);

        static SceneEditorResult EnsureSceneSession(const EditorDocumentSession& session);
        static SceneEditor* ResolveSceneEditor(IEditorContext& context, SceneEditorResult& outError);
        static bool TryResolveComponentTypeName(
            TypeQueryServices& typeQuery,
            std::string_view className,
            std::string& outResolvedTypeName,
            SceneEditorResult& outError);
    };
}
