#pragma once

#include "Core.h"
#include "Archive.h"
#include "SerializationTypes.h"

#include <string>
#include <vector>

namespace minEngine::Reflection
{
    class MEClass;
    class MEProperty;
    class MEObjectProperty;
    class MEObjectPtrProperty;
}

namespace minEngine::Serialization
{
    struct PendingObjectRef
    {
        void* ptrToPtr = nullptr;
        void* ownerObjectPtr = nullptr;
        GUID refGuid;
        const minEngine::Reflection::MEClass* expectedClass = nullptr;
        bool isRawPointer = false;
        bool expectsMEObject = false;
        std::string fieldPath;
    };

    class MINENGINE_API Serializer
    {
    public:
        static SerializeResult Serialize(const std::string& rootClassName,
                                         const void* rootObject,
                                         WriterArchive& archive,
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
                          const std::string& rootClassName,
                          const void* rootObject,
                          WriterArchive& archive,
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
                                                                     const std::string& propertyName);

        static bool IsHandlingPtr()
        {
            return m_IsHandlingPtr;
        }

        static void SetHandlingPtr(bool handling)
        {
            m_IsHandlingPtr = handling;
        }
        static bool m_IsHandlingPtr;
    };
}
