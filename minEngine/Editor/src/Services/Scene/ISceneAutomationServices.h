#pragma once

#include "Runtime/Core/Serialization/Json.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace minEngine
{
    class IEditorContext;

    struct SceneServiceResult
    {
        bool Ok = false;
        std::string ErrorCode;
        std::string Message;
        Json Result = nullptr;
        Json ExtraErrorFields = Json::object();
    };

    class ISceneQueryService
    {
    public:
        virtual ~ISceneQueryService() = default;

        virtual SceneServiceResult GetHierarchy(
            IEditorContext& context,
            std::string_view sessionId,
            int maxDepth) = 0;

        virtual SceneServiceResult GetObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId) = 0;

        virtual SceneServiceResult GetProperty(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath) = 0;
    };

    class ISceneEditService
    {
    public:
        virtual ~ISceneEditService() = default;

        virtual SceneServiceResult CreateGameObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view name,
            std::string_view parentObjectId) = 0;

        virtual SceneServiceResult DeleteGameObject(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId) = 0;

        virtual SceneServiceResult AddComponent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view className) = 0;

        virtual SceneServiceResult RemoveComponent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view componentObjectId) = 0;

        virtual SceneServiceResult SetProperty(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath,
            const Json& value) = 0;

        virtual SceneServiceResult Reparent(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view newParentObjectId) = 0;

        virtual SceneServiceResult SaveDocument(IEditorContext& context, std::string_view sessionId) = 0;
    };

    class ISceneVerifyService
    {
    public:
        virtual ~ISceneVerifyService() = default;

        virtual SceneServiceResult Validate(
            IEditorContext& context,
            std::string_view sessionId,
            std::string_view objectId,
            std::string_view propertyPath) = 0;
    };
}
