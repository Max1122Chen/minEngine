#include "Serializer.h"

#include "PrimitiveCodecRegistry.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine::Serialization
{
    using minEngine::Reflection::MEArrayProperty;
    using minEngine::Reflection::MEClass;
    using minEngine::Reflection::MEObjectProperty;
    using minEngine::Reflection::MEObjectPtrProperty;
    using minEngine::Reflection::MEObjectPtrCategory;
    using minEngine::Reflection::MEPrimitiveProperty;
    using minEngine::Reflection::MEProperty;
    using minEngine::Reflection::MEPropertyCategory;
    using minEngine::Reflection::ReflectionSystem;

    bool Serializer::m_IsHandlingPtr = false;
    std::vector<Serializer::PendingObjectRef> Serializer::m_PendingObjectRefs;

    SerializeResult Serializer::Serialize(const std::string& rootClassName,
                                                   const void* rootObject,
                                                   WriterArchive& archive,
                                                   const SerializerOptions& options)
    {
        if (rootObject == nullptr)
        {
            return SerializeResult::Failure("Serialize failed: rootObject is null.", rootClassName);
        }

        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Serialize failed: root class not found.", rootClassName);
        }

        return SerializeObjectInstance(rootClass, rootObject, archive, options, rootClassName);
    }

    SerializeResult Serializer::Deserialize(const std::string& rootClassName,
                                                     void* outRootObject,
                                                     ReaderArchive& archive,
                                                     const SerializerOptions& options)
    {
        if (outRootObject == nullptr)
        {
            return SerializeResult::Failure("Deserialize failed: outRootObject is null.", rootClassName);
        }

        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Deserialize failed: root class not found.", rootClassName);
        }

        return DeserializeObjectInstance(rootClass, outRootObject, archive, options, rootClassName);
    }

    SerializeResult Serializer::ResolvePendingObjectRefs()
    {
        if (m_PendingObjectRefs.empty())
        {
            return SerializeResult::Success();
        }

        std::vector<PendingObjectRef> unresolvedReferences;
        unresolvedReferences.reserve(m_PendingObjectRefs.size());

        size_t resolvedCount = 0;
        for (const PendingObjectRef& pendingRef : m_PendingObjectRefs)
        {
            std::shared_ptr<void> resolvedSharedPtr;
            void* resolvedRawPtr = nullptr;
            std::string resolveError;
            if (!ResolvePendingObjectRef(pendingRef, resolvedSharedPtr, resolvedRawPtr, resolveError))
            {
                unresolvedReferences.push_back(pendingRef);
                ME_CORE_WARN("Pending object reference unresolved. path='{}', guid='{}', reason='{}'",
                             pendingRef.fieldPath,
                             pendingRef.refGuid.ToString(),
                             resolveError);
                continue;
            }

            if (pendingRef.isRawPointer)
            {
                *static_cast<void**>(pendingRef.ptrToPtr) = resolvedRawPtr;
                ++resolvedCount;
                continue;
            }

            if (pendingRef.expectedClass == nullptr
                || !pendingRef.expectedClass->SetSharedPtr(resolvedSharedPtr, pendingRef.ptrToPtr))
            {
                unresolvedReferences.push_back(pendingRef);
                ME_CORE_WARN("Pending object reference assignment failed. path='{}', guid='{}'",
                             pendingRef.fieldPath,
                             pendingRef.refGuid.ToString());
                continue;
            }

            ++resolvedCount;
        }

        m_PendingObjectRefs = std::move(unresolvedReferences);

        ME_CORE_INFO("Pending object reference resolve pass finished. resolved={}, unresolved={}",
                     resolvedCount,
                     m_PendingObjectRefs.size());

        if (!m_PendingObjectRefs.empty())
        {
            return SerializeResult::Failure("Resolve pending object references finished with unresolved entries.", "PendingObjectReferences");
        }

        return SerializeResult::Success();
    }

    void Serializer::ClearPendingObjectRefs()
    {
        m_PendingObjectRefs.clear();
    }

    size_t Serializer::GetPendingObjectRefCount()
    {
        return m_PendingObjectRefs.size();
    }

    // Private methods for serialization
    SerializeResult Serializer::SerializeObjectInstance(const MEClass* classInfo,
                                                        const void* objectPtr,
                                                        WriterArchive& archive,
                                                        const SerializerOptions& options,
                                                        const std::string& path)
    {
        if (objectPtr == nullptr)
        {
            return SerializeResult::Failure("Serialize class failed: object pointer is null.", path);
        }

        if (!archive.BeginObject(classInfo->GetName()))
        {
            return SerializeResult::Failure("Serialize class failed: BeginObject returned false.", path);
        }

        // Iterate properties in the class hierarchy and serialize them
        bool iterationOk = SerializeObject_IterateProps(classInfo, objectPtr, archive, options, path);

        if (!iterationOk)
        {
            return SerializeResult::Failure("Serialize class failed during property iteration.", path);
        }

        if (!archive.EndObject())
        {
            return SerializeResult::Failure("Serialize class failed: EndObject returned false.", path);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::SerializeProperty(const MEProperty& property,
                                                           const void* valuePtr,
                                                           const void* ownerObjectPtr,
                                                           WriterArchive& archive,
                                                           const SerializerOptions& options,
                                                           const std::string& path)
    {
        switch (property.GetCategory())
        {
        case MEPropertyCategory::Primitive:
        {
            const auto* primitive = dynamic_cast<const MEPrimitiveProperty*>(&property);
            if (primitive == nullptr)
            {
                return SerializeResult::Failure("Serialize primitive failed: invalid property type.", path);
            }

            const PrimitiveCodec* codec = PrimitiveCodecRegistry::Get().Find(primitive->primitiveTypeName);
            if (codec == nullptr)
            {
                return SerializeResult::Failure("Serialize primitive failed: codec not found for type '" + primitive->primitiveTypeName + "'.", path);
            }

            if (!codec->write(archive, valuePtr))
            {
                return SerializeResult::Failure("Serialize primitive failed: codec write returned false.", path);
            }

            return SerializeResult::Success();
        }
        case MEPropertyCategory::Object:
        {
            const auto* objectProperty = dynamic_cast<const MEObjectProperty*>(&property);
            if (objectProperty == nullptr)
            {
                return SerializeResult::Failure("Serialize object failed: invalid property type.", path);
            }

            const MEClass* valueClass = objectProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Serialize object failed: value class is unresolved.", path);
            }

            return SerializeObjectInstance(valueClass, valuePtr, archive, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            // TODO: support object pointer later
            const auto* objectPtrProperty = dynamic_cast<const MEObjectPtrProperty*>(&property);
            if (objectPtrProperty == nullptr)            
            {
                return SerializeResult::Failure("Serialize object pointer failed: invalid property type.", path);
            }

            return SerializeObjectPtr(*objectPtrProperty, valuePtr, ownerObjectPtr, archive, options, path);

        }
        case MEPropertyCategory::Array:
        {
            const auto* arrayProperty = dynamic_cast<const MEArrayProperty*>(&property);
            if (arrayProperty == nullptr)
            {
                return SerializeResult::Failure("Serialize array failed: invalid property type.", path);
            }

            MEProperty* innerProperty = arrayProperty->GetInnerProperty();
            if (innerProperty == nullptr)
            {
                return SerializeResult::Failure("Serialize array failed: inner property is null.", path);
            }

            const size_t count = arrayProperty->GetSize(valuePtr);
            if (!archive.BeginArray(count))
            {
                return SerializeResult::Failure("Serialize array failed: BeginArray returned false.", path);
            }

            for (size_t index = 0; index < count; ++index)
            {
                const void* elementPtr = arrayProperty->GetConstElement(valuePtr, index);
                if (elementPtr == nullptr)
                {
                    if (!archive.WriteNull())
                    {
                        return SerializeResult::Failure("Serialize array failed: WriteNull returned false.", JoinPath(path, "[" + std::to_string(index) + "]"));
                    }
                    continue;
                }

                SerializeResult elementResult = SerializeProperty(*innerProperty,
                                                                  elementPtr,
                                                                  ownerObjectPtr,
                                                                  archive,
                                                                  options,
                                                                  JoinPath(path, "[" + std::to_string(index) + "]"));
                if (!elementResult.ok)
                {
                    return elementResult;
                }
            }

            if (!archive.EndArray())
            {
                return SerializeResult::Failure("Serialize array failed: EndArray returned false.", path);
            }

            return SerializeResult::Success();
        }
        default:
            return SerializeResult::Failure("Serialize failed: unsupported property category.", path);
        }
    }

    SerializeResult Serializer::SerializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty &objectPtrProperty, 
                                                                                    const void *ptrToPtr, 
                                                                                    const void* ownerObjectPtr,
                                                                                    WriterArchive &archive, 
                                                                                    const SerializerOptions &options, 
                                                                                    const std::string &path)
    {
        if (ptrToPtr == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: pointer to pointer is null.", path);
        }
        if (ownerObjectPtr == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: owner object pointer is null.", path);
        }

        const MEClass* staticValueClass = objectPtrProperty.GetValueClass();
        if (staticValueClass == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: value class is unresolved.", path);
        }

        MEObjectPtrCategory ptrCategory = objectPtrProperty.GetPtrCategory();
        if (ptrCategory == MEObjectPtrCategory::Invalid)
        {
            return SerializeResult::Failure("Serialize object pointer failed: invalid pointer category.", path);
        }

        (void)ptrCategory;

        const MEClass* meObjectClass = ReflectionSystem::Get().FindClass("minEngine::MEObject");
        if (meObjectClass == nullptr)
        {
            meObjectClass = ReflectionSystem::Get().FindClass("MEObject");
        }
        if (meObjectClass == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: MEObject reflection class is unresolved.", path);
        }

        if (!ReflectionSystem::Get().IsClassSameOrDerived(staticValueClass, meObjectClass))
        {
            return SerializeResult::Failure("Serialize object pointer failed: only MEObject-derived pointer properties are supported.", path);
        }

        MEObject* objectPtr = static_cast<MEObject*>(objectPtrProperty.GetMutablePointingData(const_cast<void*>(ptrToPtr)));

        if (objectPtr == nullptr)
        {
            if (!archive.WriteNull())
            {
                return SerializeResult::Failure("Serialize object pointer failed: WriteNull returned false.", path);
            }
            return SerializeResult::Success();
        }

        const MEObject* ownerObject = static_cast<const MEObject*>(ownerObjectPtr);
        const bool shouldSerializeInline = (objectPtr->GetOuter() == ownerObject);

        const MEClass* dynamicClass = objectPtr->GetClass();
        if (dynamicClass == nullptr)
        {
            dynamicClass = staticValueClass;
        }

        if (dynamicClass == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: value class is unresolved.", path);
        }

        if (!shouldSerializeInline)
        {
            GUID guid = objectPtr->GetGuid();
            if (guid.IsZero())
            {
                guid = GenerateGUID();
                objectPtr->SetGuid(guid);
            }

            if (!archive.BeginGuidRef(guid))
            {
                return SerializeResult::Failure("Serialize object pointer failed: BeginGuidRef returned false.", path);
            }

            if (!archive.EndGuidRef())
            {
                return SerializeResult::Failure("Serialize object pointer failed: EndGuidRef returned false.", path);
            }

            return SerializeResult::Success();
        }

        if (!archive.BeginObjectPtr(dynamicClass->GetName()))
        {
            return SerializeResult::Failure("Serialize class failed: BeginObjectPtr returned false.", path);
        }

        const bool iterationOk = SerializeObject_IterateProps(dynamicClass, objectPtr, archive, options, path);
        if (!iterationOk)
        {
            return SerializeResult::Failure("Serialize class failed during property iteration.", path);
        }

        if (!archive.EndObjectPtr())
        {
            return SerializeResult::Failure("Serialize class failed: EndObjectPtr returned false.", path);
        }

        return SerializeResult::Success();
    }

    bool Serializer::SerializeObject_IterateProps(const minEngine::Reflection::MEClass *classInfo, 
                                                                            const void *objectPtr, 
                                                                            WriterArchive &archive, 
                                                                            const SerializerOptions &options, 
                                                                            const std::string &path)
    {
        SerializeResult result = SerializeResult::Success();
        return ReflectionSystem::Get().ForEachPropertyInHierarchy(
        classInfo->GetName(),
        [&](const MEProperty& property) -> bool
        {
            if (property.GetConstAccessor() == nullptr)
            {
                result = SerializeResult::Failure("Serialize property failed: const accessor is null.", JoinPath(path, property.GetName()));
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            const void* valuePtr = property.GetConst(objectPtr);
            if (valuePtr == nullptr)
            {
                result = SerializeResult::Failure("Serialize property failed: value pointer is null.", JoinPath(path, property.GetName()));
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            if (!archive.BeginField(property.GetName()))
            {
                result = SerializeResult::Failure("Serialize property failed: BeginField returned false.", JoinPath(path, property.GetName()));
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            // Pass ownerObjectPtr to SerializeProperty for potential use in serializing object pointer property
            result = SerializeProperty(property, valuePtr, objectPtr, archive, options, JoinPath(path, property.GetName()));
            if (!result.ok)
            {
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            if (!archive.EndField())
            {
                result = SerializeResult::Failure("Serialize property failed: EndField returned false.", JoinPath(path, property.GetName()));
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            return true;
        });
    }

    // Private methods for deserialization
    SerializeResult Serializer::DeserializeObjectInstance(const MEClass* classInfo,
                                                          void* objectPtr,
                                                          ReaderArchive& archive,
                                                          const SerializerOptions& options,
                                                          const std::string& path)
    {
        if (objectPtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize class failed: object pointer is null.", path);
        }
        
        if (!archive.BeginObject(classInfo->GetName()))
        {
            return SerializeResult::Failure("Deserialize class failed: BeginObject returned false.", path);
        }

        // Iterate the properties in hierarchy and deserialize each property.
        const bool iterationOk = DeserializeObject_IterateProps(classInfo, objectPtr, archive, options, path);

        if (!iterationOk)
        {
            return SerializeResult::Failure("Deserialize class failed during property iteration.", path);
        }

        if (!archive.EndObject())
        {
            return SerializeResult::Failure("Deserialize class failed: EndObject returned false.", path);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::DeserializeProperty(const MEProperty& property,
                                                          void* outValuePtr,
                                                          void* ownerObjectPtr,
                                                          ReaderArchive& archive,
                                                          const SerializerOptions& options,
                                                          const std::string& path)
    {
        switch (property.GetCategory())
        {
        case MEPropertyCategory::Primitive:
        {
            const auto* primitive = dynamic_cast<const MEPrimitiveProperty*>(&property);
            if (primitive == nullptr)
            {
                return SerializeResult::Failure("Deserialize primitive failed: invalid property type.", path);
            }

            const PrimitiveCodec* codec = PrimitiveCodecRegistry::Get().Find(primitive->primitiveTypeName);
            if (codec == nullptr)
            {
                return SerializeResult::Failure("Deserialize primitive failed: codec not found for type '" + primitive->primitiveTypeName + "'.", path);
            }

            if (!codec->read(archive, outValuePtr))
            {
                return SerializeResult::Failure("Deserialize primitive failed: codec read returned false.", path);
            }

            return SerializeResult::Success();
        }
        case MEPropertyCategory::Object:
        {
            const auto* objectProperty = dynamic_cast<const MEObjectProperty*>(&property);
            if (objectProperty == nullptr)
            {
                return SerializeResult::Failure("Deserialize object failed: invalid property type.", path);
            }

            const MEClass* valueClass = objectProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Deserialize object failed: value class is unresolved.", path);
            }

            return DeserializeObjectInstance(valueClass, outValuePtr, archive, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            // TODO: support object pointer later
            const auto* objectPtrProperty = dynamic_cast<const MEObjectPtrProperty*>(&property);
            if (objectPtrProperty == nullptr)            
            {
                return SerializeResult::Failure("Deserialize object pointer failed: invalid property type.", path);
            }

            const MEClass* valueClass = objectPtrProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Deserialize object pointer failed: value class is unresolved.", path);
            }

            // outValuePtr actually serves as a pointer to pointer for object pointer property, we need to dereference it first to get the current pointer value.
            return DeserializeObjectPtr(*objectPtrProperty, outValuePtr, ownerObjectPtr, archive, options, path);

        }
        case MEPropertyCategory::Array:
        {
            const auto* arrayProperty = dynamic_cast<const MEArrayProperty*>(&property);
            if (arrayProperty == nullptr)
            {
                return SerializeResult::Failure("Deserialize array failed: invalid property type.", path);
            }

            MEProperty* innerProperty = arrayProperty->GetInnerProperty();
            if (innerProperty == nullptr)
            {
                return SerializeResult::Failure("Deserialize array failed: inner property is null.", path);
            }

            size_t count = 0;
            if (!archive.BeginArray(count))
            {
                return SerializeResult::Failure("Deserialize array failed: BeginArray returned false.", path);
            }

            arrayProperty->Resize(outValuePtr, count);
            for (size_t index = 0; index < count; ++index)
            {
                if (!archive.EnterArrayElement(index))
                {
                    return SerializeResult::Failure("Deserialize array failed: EnterArrayElement returned false.", JoinPath(path, "[" + std::to_string(index) + "]"));
                }

                void* elementPtr = arrayProperty->GetMutableElement(outValuePtr, index);
                if (elementPtr == nullptr)
                {
                    return SerializeResult::Failure("Deserialize array failed: mutable element pointer is null.", JoinPath(path, "[" + std::to_string(index) + "]"));
                }

                SerializeResult elementResult = DeserializeProperty(*innerProperty,
                                                                    elementPtr,
                                                                    ownerObjectPtr,
                                                                    archive,
                                                                    options,
                                                                    JoinPath(path, "[" + std::to_string(index) + "]"));

                if (!archive.LeaveArrayElement())
                {
                    return SerializeResult::Failure("Deserialize array failed: LeaveArrayElement returned false.", JoinPath(path, "[" + std::to_string(index) + "]"));
                }

                if (!elementResult.ok)
                {
                    return elementResult;
                }
            }

            if (!archive.EndArray())
            {
                return SerializeResult::Failure("Deserialize array failed: EndArray returned false.", path);
            }

            return SerializeResult::Success();
        }
        default:
            return SerializeResult::Failure("Deserialize failed: unsupported property category.", path);
        }
    }

    SerializeResult Serializer::DeserializeObjectPtr(const minEngine::Reflection::MEObjectPtrProperty &objectPtrProperty,
                                                                                void *ptrToPtr,
                                                                                void* ownerObjectPtr,
                                                                                ReaderArchive &archive, 
                                                                                const SerializerOptions &options, 
                                                                                const std::string &path)
    {
        if (ptrToPtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize class failed: object pointer is null.", path);
        }

        if (ownerObjectPtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize object pointer failed: owner object pointer is null.", path);
        }
        
        const MEClass* classInfo = objectPtrProperty.GetValueClass();
        if (classInfo == nullptr)
        {
            return SerializeResult::Failure("Deserialize object pointer failed: value class is unresolved.", path);
        }

        MEObjectPtrCategory ptrCategory = objectPtrProperty.GetPtrCategory();
        if (ptrCategory == MEObjectPtrCategory::Invalid)
        {
            return SerializeResult::Failure("Deserialize object pointer failed: invalid pointer category.", path);
        }

        const MEClass* meObjectClass = ReflectionSystem::Get().FindClass("minEngine::MEObject");
        if (meObjectClass == nullptr)
        {
            meObjectClass = ReflectionSystem::Get().FindClass("MEObject");
        }
        const bool supportsMEObject = (meObjectClass != nullptr) && ReflectionSystem::Get().IsClassSameOrDerived(classInfo, meObjectClass);

        if (archive.ReadNull())
        {
            if (ptrCategory == MEObjectPtrCategory::Raw)
            {
                *static_cast<void**>(ptrToPtr) = nullptr;
                return SerializeResult::Success();
            }

            if (!classInfo->SetSharedPtr(std::shared_ptr<void>{}, ptrToPtr))
            {
                return SerializeResult::Failure("Deserialize object pointer failed: failed to reset shared pointer.", path);
            }

            return SerializeResult::Success();
        }

        std::string dynamicClassName;
        if (archive.BeginObjectPtr(classInfo, dynamicClassName))
        {
            const MEClass* dynamicClassInfo = classInfo;
            if (!dynamicClassName.empty())
            {
                dynamicClassInfo = ReflectionSystem::Get().FindClass(dynamicClassName);
                if (dynamicClassInfo == nullptr)
                {
                    const bool closed = archive.EndObjectPtr();
                    if (!closed)
                    {
                        return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after unresolved dynamic type.", path);
                    }
                    return SerializeResult::Failure("Deserialize object pointer failed: dynamic class not found.", path);
                }
            }

            std::shared_ptr<void> newObjectPtr = dynamicClassInfo->CreateInstance();
            if (newObjectPtr == nullptr)
            {
                const bool closed = archive.EndObjectPtr();
                if (!closed)
                {
                    return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after factory failure.", path);
                }
                return SerializeResult::Failure("Deserialize object pointer failed: failed to create dynamic object instance.", path);
            }

            void* objectPtr = newObjectPtr.get();
            std::shared_ptr<MEObject> managedObject;
            bool registeredManagedObject = false;
            if (supportsMEObject)
            {
                MEObject* meObjectPtr = static_cast<MEObject*>(objectPtr);
                meObjectPtr->SetClass(dynamicClassInfo);
                meObjectPtr->SetOuter(static_cast<MEObject*>(ownerObjectPtr));

                managedObject = std::static_pointer_cast<MEObject>(newObjectPtr);
                ObjectManager::Get().RegisterObject(managedObject);
                registeredManagedObject = true;
            }

            const bool iterationOk = DeserializeObject_IterateProps(dynamicClassInfo, objectPtr, archive, options, path);
            if (!iterationOk)
            {
                if (registeredManagedObject)
                {
                    ObjectManager::Get().UnregisterObject(managedObject.get());
                }

                const bool closed = archive.EndObjectPtr();
                if (!closed)
                {
                    return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after property iteration failure.", path);
                }
                return SerializeResult::Failure("Deserialize class failed during property iteration.", path);
            }

            if (ptrCategory == MEObjectPtrCategory::Raw)
            {
                if (!supportsMEObject)
                {
                    if (registeredManagedObject)
                    {
                        ObjectManager::Get().UnregisterObject(managedObject.get());
                    }

                    const bool closed = archive.EndObjectPtr();
                    if (!closed)
                    {
                        return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false for unsupported raw pointer type.", path);
                    }
                    return SerializeResult::Failure("Deserialize object pointer failed: inline raw pointer requires MEObject-derived type.", path);
                }

                *static_cast<void**>(ptrToPtr) = objectPtr;
            }
            else
            {
                if (!classInfo->SetSharedPtr(newObjectPtr, ptrToPtr))
                {
                    if (registeredManagedObject)
                    {
                        ObjectManager::Get().UnregisterObject(managedObject.get());
                    }

                    const bool closed = archive.EndObjectPtr();
                    if (!closed)
                    {
                        return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after assigning shared pointer.", path);
                    }
                    return SerializeResult::Failure("Deserialize object pointer failed: failed to assign shared pointer.", path);
                }
            }

            if (!archive.EndObjectPtr())
            {
                return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false.", path);
            }

            return SerializeResult::Success();
        }

        GUID referenceGuid;
        if (!archive.BeginGuidRef(referenceGuid))
        {
            return SerializeResult::Failure("Deserialize object pointer failed: neither inline object node nor GUID reference node was found.", path);
        }

        if (referenceGuid.IsZero())
        {
            const bool closed = archive.EndGuidRef();
            if (!closed)
            {
                return SerializeResult::Failure("Deserialize object pointer failed: EndGuidRef returned false after invalid GUID.", path);
            }
            return SerializeResult::Failure("Deserialize object pointer failed: reference GUID is zero.", path);
        }

        PendingObjectRef pendingRef;
        pendingRef.ptrToPtr = ptrToPtr;
        pendingRef.ownerObjectPtr = ownerObjectPtr;
        pendingRef.refGuid = referenceGuid;
        pendingRef.expectedClass = classInfo;
        pendingRef.isRawPointer = (ptrCategory == MEObjectPtrCategory::Raw);
        pendingRef.expectsMEObject = supportsMEObject;
        pendingRef.fieldPath = path;
        m_PendingObjectRefs.push_back(std::move(pendingRef));

        if (ptrCategory == MEObjectPtrCategory::Raw)
        {
            *static_cast<void**>(ptrToPtr) = nullptr;
        }
        else
        {
            if (!classInfo->SetSharedPtr(std::shared_ptr<void>{}, ptrToPtr))
            {
                const bool closed = archive.EndGuidRef();
                if (!closed)
                {
                    return SerializeResult::Failure("Deserialize object pointer failed: EndGuidRef returned false after shared pointer reset failure.", path);
                }
                return SerializeResult::Failure("Deserialize object pointer failed: failed to reset shared pointer before deferred resolve.", path);
            }
        }

        if (!archive.EndGuidRef())
        {
            return SerializeResult::Failure("Deserialize object pointer failed: EndGuidRef returned false.", path);
        }

        return SerializeResult::Success();
    }

    bool Serializer::DeserializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                                void* objectPtr,
                                                ReaderArchive& archive,
                                                const SerializerOptions& options,
                                                const std::string& path)
    {
        SerializeResult result = SerializeResult::Success();
        return ReflectionSystem::Get().ForEachPropertyInHierarchy(
        classInfo->GetName(),
        [&](const MEProperty& property) -> bool
        {
            const std::string propertyPath = JoinPath(path, property.GetName());
            const bool hasField = archive.EnterField(property.GetName());
            if (!hasField)
            {
                if (options.skipUnknownField)
                {
                    return true;
                }

                result = SerializeResult::Failure("Deserialize property failed: missing field.", propertyPath);
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            if (property.GetMutableAccessor() == nullptr)
            {
                result = SerializeResult::Failure("Deserialize property failed: mutable accessor is null.", propertyPath);
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            void* valuePtr = property.GetMutable(objectPtr);
            if (valuePtr == nullptr)
            {
                result = SerializeResult::Failure("Deserialize property failed: value pointer is null.", propertyPath);
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            result = DeserializeProperty(property, valuePtr, objectPtr, archive, options, propertyPath);
            if (!result.ok)
            {
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            const bool leaveOk = archive.LeaveField();
            if (!leaveOk)
            {
                result = SerializeResult::Failure("Deserialize property failed: LeaveField returned false.", propertyPath);
                ME_CORE_ERROR(result.message, result.fieldPath);
                return false;
            }

            return result.ok;
        });
    }

    bool Serializer::ResolvePendingObjectRef(const PendingObjectRef& pendingRef,
                                                         std::shared_ptr<void>& outResolvedSharedPtr,
                                                         void*& outResolvedRawPtr,
                                                         std::string& outErrorMessage)
    {
        outResolvedSharedPtr.reset();
        outResolvedRawPtr = nullptr;
        outErrorMessage.clear();

        if (pendingRef.ptrToPtr == nullptr)
        {
            outErrorMessage = "ptrToPtr is null";
            return false;
        }

        if (pendingRef.refGuid.IsZero())
        {
            outErrorMessage = "reference guid is zero";
            return false;
        }

        std::shared_ptr<MEObject> trackedObject = minEngine::FindObject(pendingRef.refGuid);
        if (trackedObject != nullptr)
        {
            if (pendingRef.expectsMEObject && pendingRef.expectedClass != nullptr)
            {
                const MEClass* trackedClass = trackedObject->GetClass();
                if (trackedClass != nullptr && !ReflectionSystem::Get().IsClassSameOrDerived(trackedClass, pendingRef.expectedClass))
                {
                    outErrorMessage = "resolved object type mismatch";
                    return false;
                }
            }

            outResolvedSharedPtr = std::static_pointer_cast<void>(trackedObject);
            outResolvedRawPtr = trackedObject.get();
            return true;
        }

        const AssetMeta* assetMeta = AssetManager::Get().FindAssetMetaByGuid(pendingRef.refGuid);
        if (assetMeta == nullptr)
        {
            outErrorMessage = "guid not found in object manager or asset registry";
            return false;
        }

        if (assetMeta->AssetType == "StaticMesh")
        {
            std::shared_ptr<StaticMesh> staticMesh = AssetManager::Get().LoadStaticMeshByMeta(*assetMeta);
            if (staticMesh == nullptr)
            {
                outErrorMessage = "failed to load static mesh by guid";
                return false;
            }

            outResolvedSharedPtr = std::static_pointer_cast<void>(staticMesh);
            outResolvedRawPtr = staticMesh.get();
            return true;
        }

        if (assetMeta->AssetType == "Texture2D")
        {
            std::shared_ptr<Texture2D> texture = AssetManager::Get().LoadTexture2DByMeta(*assetMeta, 0);
            if (texture == nullptr)
            {
                outErrorMessage = "failed to load texture by guid";
                return false;
            }

            outResolvedSharedPtr = std::static_pointer_cast<void>(texture);
            outResolvedRawPtr = texture.get();
            return true;
        }

        outErrorMessage = "unsupported asset type '" + assetMeta->AssetType + "'";
        return false;
    }

    std::string Serializer::JoinPath(const std::string& basePath, const std::string& nextSegment)
    {
        if (basePath.empty())
        {
            return nextSegment;
        }

        if (!nextSegment.empty() && nextSegment.front() == '[')
        {
            return basePath + nextSegment;
        }

        return basePath + "." + nextSegment;
    }
}
