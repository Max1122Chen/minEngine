#pragma once

#include "Core.h"
#include "Archive.h"
#include "Json.h"
#include "SerializationTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
    class MEProperty;
    class MEObjectProperty;
    class MEObjectPtrProperty;
}

namespace minEngine
{
    struct SceneCloneContext;
}

namespace minEngine::Serialization
{
    struct PendingObjectRef
    {
        void* ptrToPtr = nullptr;
        void* ownerObjectPtr = nullptr;
        GUID refGuid;
        const minEngine::Reflection::MEClass* expectedClass = nullptr;
        const minEngine::Reflection::MEProperty* property = nullptr;
        bool isRawPointer = false;
        bool expectsMEObject = false;
        std::string fieldPath;
    };

    class MINENGINE_API Serializer
    {
    public:
        static SerializeResult Serialize(const Reflection::MEClass* rootClass,
                                         const void* rootObject,
                                         WriterArchive& archive,
                                         const SerializerOptions& options = SerializerOptions{});

        static SerializeResult Serialize(const std::string& rootClassName,
                                         const void* rootObject,
                                         WriterArchive& archive,
                                         const SerializerOptions& options = SerializerOptions{});

        static SerializeResult Deserialize(const Reflection::MEClass* rootClass,
                                           void* outRootObject,
                                           ReaderArchive& archive,
                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                           const SerializerOptions& options = SerializerOptions{});

        static SerializeResult Deserialize(const std::string& rootClassName,
                                           void* outRootObject,
                                           ReaderArchive& archive,
                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                           const SerializerOptions& options = SerializerOptions{});

        // Resolve GUID-based object references captured during deserialization.
        // This should be called manually after a load unit (scene/material/etc.) finishes.
        static SerializeResult ResolvePendingObjectRefs(std::vector<PendingObjectRef>& unresolvedRefs);

        static SerializeResult ToFile(const std::string& filePath,
                                      const Reflection::MEClass* rootClass,
                                      const void* rootObject,
                                      WriterArchive& archive,
                                      const SerializerOptions& options = SerializerOptions{});

        static SerializeResult ToFile(const std::string& filePath,
                                      const std::string& rootClassName,
                                      const void* rootObject,
                                      WriterArchive& archive,
                                      const SerializerOptions& options = SerializerOptions{});

        static SerializeResult FromFile(const std::string& filePath,
                                        const Reflection::MEClass* rootClass,
                                        void* outRootObject,
                                        ReaderArchive& archive,
                                        const SerializerOptions& options = SerializerOptions{});

        static SerializeResult FromFile(const std::string& filePath,
                                        const std::string& rootClassName,
                                        void* outRootObject,
                                        ReaderArchive& archive,
                                        const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializeProperty(void* ownerObject,
                                                 const Reflection::MEClass* ownerClass,
                                                 const std::string& propertyName,
                                                 WriterArchive& archive,
                                                 const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializeProperty(void* ownerObject,
                                                   const Reflection::MEClass* ownerClass,
                                                   const std::string& propertyName,
                                                   ReaderArchive& archive,
                                                   std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                   const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializePropertyToBuffer(void* ownerObject,
                                                         const Reflection::MEClass* ownerClass,
                                                         const std::string& propertyName,
                                                         std::vector<uint8_t>& outBuffer,
                                                         const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializePropertyFromBuffer(void* ownerObject,
                                                             const Reflection::MEClass* ownerClass,
                                                             const std::string& propertyName,
                                                             const std::vector<uint8_t>& buffer,
                                                             std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                             const SerializerOptions& options = SerializerOptions{});

        // Serialize/deserialize a nested property path like "Transform.Position".
        // This is used by editor undo/redo to provide per-field commands for nested structs.
        static SerializeResult SerializePropertyByPathToBuffer(void* ownerObject,
                                                              const Reflection::MEClass* ownerClass,
                                                              const std::string& propertyPath,
                                                              std::vector<uint8_t>& outBuffer,
                                                              const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializePropertyByPathFromBuffer(void* ownerObject,
                                                                  const Reflection::MEClass* ownerClass,
                                                                  const std::string& propertyPath,
                                                                  const std::vector<uint8_t>& buffer,
                                                                  std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                                  const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializeObjectToBuffer(const Reflection::MEClass* rootClass,
                                                       const void* rootObject,
                                                       std::vector<uint8_t>& outBuffer,
                                                       const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializeObjectToBuffer(const std::string& rootClassName,
                                                       const void* rootObject,
                                                       std::vector<uint8_t>& outBuffer,
                                                       const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializeObjectFromBuffer(const Reflection::MEClass* rootClass,
                                                           void* outRootObject,
                                                           const std::vector<uint8_t>& buffer,
                                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                           const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializeObjectFromBuffer(const std::string& rootClassName,
                                                           void* outRootObject,
                                                           const std::vector<uint8_t>& buffer,
                                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                           const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializeObjectToJson(const Reflection::MEClass* rootClass,
                                                     const void* rootObject,
                                                     Json& outRoot,
                                                     const SerializerOptions& options = SerializerOptions{});

        static SerializeResult SerializeObjectToJson(const std::string& rootClassName,
                                                     const void* rootObject,
                                                     Json& outRoot,
                                                     const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializeObjectFromJson(const Reflection::MEClass* rootClass,
                                                         void* outRootObject,
                                                         const Json& root,
                                                         std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                         const SerializerOptions& options = SerializerOptions{});

        static SerializeResult DeserializeObjectFromJson(const std::string& rootClassName,
                                                         void* outRootObject,
                                                         const Json& root,
                                                         std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                         const SerializerOptions& options = SerializerOptions{});

        template <typename T>
        static SerializeResult Serialize(const T* rootObject,
                                         WriterArchive& archive,
                                         const SerializerOptions& options = SerializerOptions{})
        {
            return Serialize(T::StaticClass(), static_cast<const void*>(rootObject), archive, options);
        }

        template <typename T>
        static SerializeResult Deserialize(T* outRootObject,
                                           ReaderArchive& archive,
                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                           const SerializerOptions& options = SerializerOptions{})
        {
            return Deserialize(T::StaticClass(), static_cast<void*>(outRootObject), archive, outUnresolvedRefs, options);
        }

        template <typename T>
        static SerializeResult ToFile(const std::string& filePath,
                                      const T* rootObject,
                                      WriterArchive& archive,
                                      const SerializerOptions& options = SerializerOptions{})
        {
            return ToFile(filePath, T::StaticClass(), static_cast<const void*>(rootObject), archive, options);
        }

        template <typename T>
        static SerializeResult FromFile(const std::string& filePath,
                                        T* outRootObject,
                                        ReaderArchive& archive,
                                        const SerializerOptions& options = SerializerOptions{})
        {
            return FromFile(filePath, T::StaticClass(), static_cast<void*>(outRootObject), archive, options);
        }

        template <typename T>
        static SerializeResult SerializeObjectToBuffer(const T* rootObject,
                                                       std::vector<uint8_t>& outBuffer,
                                                       const SerializerOptions& options = SerializerOptions{})
        {
            return SerializeObjectToBuffer(T::StaticClass(), static_cast<const void*>(rootObject), outBuffer, options);
        }

        template <typename T>
        static SerializeResult DeserializeObjectFromBuffer(T* outRootObject,
                                                           const std::vector<uint8_t>& buffer,
                                                           std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                           const SerializerOptions& options = SerializerOptions{})
        {
            return DeserializeObjectFromBuffer(
                T::StaticClass(),
                static_cast<void*>(outRootObject),
                buffer,
                outUnresolvedRefs,
                options);
        }

        template <typename T>
        static SerializeResult SerializeObjectToJson(const T* rootObject,
                                                     Json& outRoot,
                                                     const SerializerOptions& options = SerializerOptions{})
        {
            return SerializeObjectToJson(T::StaticClass(), static_cast<const void*>(rootObject), outRoot, options);
        }

        template <typename T>
        static SerializeResult DeserializeObjectFromJson(T* outRootObject,
                                                         const Json& root,
                                                         std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                         const SerializerOptions& options = SerializerOptions{})
        {
            return DeserializeObjectFromJson(
                T::StaticClass(),
                static_cast<void*>(outRootObject),
                root,
                outUnresolvedRefs,
                options);
        }

        static void SetActiveCloneContext(SceneCloneContext* cloneContext);
        static SceneCloneContext* GetActiveCloneContext();

    private:
        static SerializeResult SerializeObjectInstance(const minEngine::Reflection::MEClass* classInfo,
                                              const void* objectPtr,
                                              WriterArchive& archive,
                                              const SerializerOptions& options,
                                              const std::string& path);

        static SerializeResult SerializeProperty(const minEngine::Reflection::MEProperty& property,
                                                 const Reflection::PropertySpecifierMask propertySpecifierMask,
                                                 const void* valuePtr,
                                                 const void* ownerObjectPtr,
                                                 WriterArchive& archive,
                                                 const SerializerOptions& options,
                                                 const std::string& path);

        static SerializeResult SerializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                 const Reflection::PropertySpecifierMask propertySpecifierMask,
                                                 const void* ptrToPtr,
                                                 const void* ownerObjectPtr,
                                                 WriterArchive& archive,
                                                 const SerializerOptions& options,
                                                 const std::string& path);

        static SerializeResult SerializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                        const void* objectPtr,
                                        WriterArchive& archive,
                                        const SerializerOptions& options,
                                        const std::string& path);

        static SerializeResult DeserializeObjectInstance(const minEngine::Reflection::MEClass* classInfo,
                                                void* objectPtr,
                                                ReaderArchive& archive,
                                                std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                const SerializerOptions& options,
                                                const std::string& path);

        static SerializeResult DeserializeProperty(const minEngine::Reflection::MEProperty& property,
                                                   void* outValuePtr,
                                                   void* ownerObjectPtr,
                                                   ReaderArchive& archive,
                                                   std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                   const SerializerOptions& options,
                                                   const std::string& path);

        static SerializeResult DeserializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty& objectPtrProperty,
                                                   void* ptrToPtr,
                                                   void* ownerObjectPtr,
                                                   ReaderArchive& archive,
                                                   std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                   const SerializerOptions& options,
                                                   const std::string& path);

        static SerializeResult DeserializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                                void* objectPtr,
                                                ReaderArchive& archive,
                                                std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                const SerializerOptions& options,
                                                const std::string& path);

        static bool ResolvePendingObjectRef(const PendingObjectRef& pendingRef,
                                std::shared_ptr<void>& outResolvedSharedPtr,
                                void*& outResolvedRawPtr,
                                std::string& outErrorMessage);

        static bool ResolvePendingAssetRef(const PendingObjectRef& pendingRef,
                                std::shared_ptr<void>& outResolvedSharedPtr,
                                void*& outResolvedRawPtr,
                                std::string& outErrorMessage);

        static std::string JoinPath(const std::string& basePath, const std::string& nextSegment);

        static const Reflection::MEProperty* FindPropertyInHierarchy(const Reflection::MEClass* ownerClass,
                                                                     std::string_view propertyName);

        static SceneCloneContext* s_ActiveCloneContext;
    };
}
