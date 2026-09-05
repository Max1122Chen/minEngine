#include "Serializer.h"

#include "BinaryArchive.h"
#include "JsonArchive.h"
#include "PrimitiveCodecRegistry.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/PropertyAssign.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Scene/SceneCloneContext.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace minEngine::Serialization
{
    using minEngine::Reflection::AssignProperty;
    using minEngine::Reflection::MEArrayProperty;
    using minEngine::Reflection::MEClass;
    using minEngine::Reflection::MEObjectProperty;
    using minEngine::Reflection::MEObjectPtrProperty;
    using minEngine::Reflection::MEObjectPtrCategory;
    using minEngine::Reflection::MEPrimitiveProperty;
    using minEngine::Reflection::MEProperty;
    using minEngine::Reflection::MEPropertyCategory;
    using minEngine::Reflection::PropertySpecifier;
    using minEngine::Reflection::PropertySpecifierMask;
    using minEngine::Reflection::ReflectionSystem;

    struct AlignedPropertyTemp
    {
        std::vector<uint8_t> storage;
        void* ptr = nullptr;

        bool Allocate(const MEProperty& property)
        {
            const size_t size = property.GetStorageSize();
            const size_t align = std::max<size_t>(property.GetStorageAlignment(), static_cast<size_t>(1));
            if (size == 0)
            {
                return false;
            }

            storage.resize(size + align);
            const uintptr_t addr = reinterpret_cast<uintptr_t>(storage.data());
            const uintptr_t aligned = (addr + (align - 1)) & ~(static_cast<uintptr_t>(align) - 1);
            ptr = reinterpret_cast<void*>(aligned);

            if (property.GetValueConstructFn() != nullptr)
            {
                property.ConstructValue(ptr);
            }
            else
            {
                std::memset(ptr, 0, size);
            }

            return true;
        }

        void Destroy(const MEProperty& property)
        {
            if (ptr != nullptr && property.GetValueDestructFn() != nullptr)
            {
                property.DestructValue(ptr);
            }

            ptr = nullptr;
        }
    };

    static bool WriteRawObjectPtrValue(void* ownerObjectPtr,
                                       const MEObjectPtrProperty& objectPtrProperty,
                                       void* ptrToPtr,
                                       void* value)
    {
        if (objectPtrProperty.HasPropertySetter() && ownerObjectPtr != nullptr)
        {
            return AssignProperty(ownerObjectPtr, objectPtrProperty, &value);
        }

        if (ptrToPtr == nullptr)
        {
            return false;
        }

        *static_cast<void**>(ptrToPtr) = value;
        return true;
    }

    static const MEProperty* FindPropertyInHierarchyShared(const MEClass* ownerClass, std::string_view propertyName)
    {
        if (ownerClass == nullptr || propertyName.empty())
        {
            return nullptr;
        }

        const MEProperty* foundProperty = nullptr;
        ReflectionSystem::Get().ForEachPropertyInHierarchy(
            ownerClass,
            [&](const MEProperty& property) -> bool
            {
                if (property.GetName() == propertyName)
                {
                    foundProperty = &property;
                    return false;
                }
                return true;
            });

        return foundProperty;
    }

    static bool SplitPropertyPath(std::string_view propertyPath, std::vector<std::string_view>& outSegments)
    {
        outSegments.clear();
        if (propertyPath.empty())
        {
            return false;
        }

        size_t start = 0;
        while (start < propertyPath.size())
        {
            const size_t dot = propertyPath.find('.', start);
            const size_t end = dot == std::string_view::npos ? propertyPath.size() : dot;
            const std::string_view segment = propertyPath.substr(start, end - start);
            if (segment.empty())
            {
                return false;
            }

            outSegments.push_back(segment);

            if (dot == std::string_view::npos)
            {
                break;
            }

            start = dot + 1;
        }

        return !outSegments.empty();
    }

    static SerializeResult WalkToOwningObjectByPath(void*& inOutOwnerObject,
                                                   const MEClass*& inOutOwnerClass,
                                                   const std::vector<std::string_view>& segments,
                                                   const std::string& fullPathForErrors,
                                                   bool isMutable)
    {
        auto FindPropertyInHierarchyLocal = [](const MEClass* ownerClass, std::string_view propertyName) -> const MEProperty*
        {
            return FindPropertyInHierarchyShared(ownerClass, propertyName);
        };

        if (inOutOwnerObject == nullptr || inOutOwnerClass == nullptr)
        {
            return SerializeResult::Failure("PropertyByPath failed: owner is null.", fullPathForErrors);
        }

        if (segments.size() < 2)
        {
            return SerializeResult::Success();
        }

        for (size_t i = 0; i + 1 < segments.size(); ++i)
        {
            const std::string_view segment = segments[i];
            const MEProperty* property = FindPropertyInHierarchyLocal(inOutOwnerClass, segment);
            if (property == nullptr)
            {
                return SerializeResult::Failure("PropertyByPath failed: segment not found.", fullPathForErrors);
            }

            if (property->GetCategory() != MEPropertyCategory::Object)
            {
                return SerializeResult::Failure("PropertyByPath failed: non-object segment in path.", fullPathForErrors);
            }

            const MEObjectProperty& objectProperty = static_cast<const MEObjectProperty&>(*property);
            const MEClass* valueClass = objectProperty.GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("PropertyByPath failed: value class unresolved.", fullPathForErrors);
            }

            void* nextObjectPtr = nullptr;
            if (isMutable)
            {
                if (property->GetMutableAccessor() == nullptr)
                {
                    return SerializeResult::Failure("PropertyByPath failed: mutable accessor is null.", fullPathForErrors);
                }

                nextObjectPtr = property->GetMutable(inOutOwnerObject);
            }
            else
            {
                if (property->GetConstAccessor() == nullptr)
                {
                    return SerializeResult::Failure("PropertyByPath failed: const accessor is null.", fullPathForErrors);
                }

                nextObjectPtr = const_cast<void*>(property->GetConst(inOutOwnerObject));
            }

            if (nextObjectPtr == nullptr)
            {
                return SerializeResult::Failure("PropertyByPath failed: intermediate object pointer is null.", fullPathForErrors);
            }

            inOutOwnerObject = nextObjectPtr;
            inOutOwnerClass = valueClass;
        }

        return SerializeResult::Success();
    }

    SceneCloneContext* Serializer::s_ActiveCloneContext = nullptr;

    void Serializer::SetActiveCloneContext(SceneCloneContext* cloneContext)
    {
        s_ActiveCloneContext = cloneContext;
    }

    SceneCloneContext* Serializer::GetActiveCloneContext()
    {
        return s_ActiveCloneContext;
    }

    SerializeResult Serializer::Serialize(const MEClass* rootClass,
                                          const void* rootObject,
                                          WriterArchive& archive,
                                          const SerializerOptions& options)
    {
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Serialize failed: rootClass is null.");
        }

        if (rootObject == nullptr)
        {
            return SerializeResult::Failure("Serialize failed: rootObject is null.", rootClass->GetName());
        }

        return SerializeObjectInstance(rootClass, rootObject, archive, options, rootClass->GetName());
    }

    SerializeResult Serializer::Serialize(const std::string& rootClassName,
                                          const void* rootObject,
                                          WriterArchive& archive,
                                          const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Serialize failed: root class not found.", rootClassName);
        }

        return Serialize(rootClass, rootObject, archive, options);
    }

    SerializeResult Serializer::Deserialize(const MEClass* rootClass,
                                            void* outRootObject,
                                            ReaderArchive& archive,
                                            std::vector<PendingObjectRef>& outUnresolvedRefs,
                                            const SerializerOptions& options)
    {
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Deserialize failed: rootClass is null.");
        }

        if (outRootObject == nullptr)
        {
            return SerializeResult::Failure("Deserialize failed: outRootObject is null.", rootClass->GetName());
        }

        return DeserializeObjectInstance(rootClass, outRootObject, archive, outUnresolvedRefs, options, rootClass->GetName());
    }

    SerializeResult Serializer::Deserialize(const std::string& rootClassName,
                                            void* outRootObject,
                                            ReaderArchive& archive,
                                            std::vector<PendingObjectRef>& outUnresolvedRefs,
                                            const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("Deserialize failed: root class not found.", rootClassName);
        }

        return Deserialize(rootClass, outRootObject, archive, outUnresolvedRefs, options);
    }

    SerializeResult Serializer::ResolvePendingObjectRefs(std::vector<PendingObjectRef>& unresolvedRefs)
    {
        if (unresolvedRefs.empty())
        {
            return SerializeResult::Success();
        }

        std::vector<PendingObjectRef> remainingRefs;
        remainingRefs.reserve(unresolvedRefs.size());

        size_t resolvedCount = 0;
        for (const PendingObjectRef& pendingRef : unresolvedRefs)
        {
            std::shared_ptr<void> resolvedSharedPtr;
            void* resolvedRawPtr = nullptr;
            std::string resolveError;
            if (!ResolvePendingObjectRef(pendingRef, resolvedSharedPtr, resolvedRawPtr, resolveError))
            {
                remainingRefs.push_back(pendingRef);
                ME_CORE_WARN("Pending object reference unresolved. path='{}', guid='{}', reason='{}'",
                             pendingRef.fieldPath,
                             pendingRef.refGuid.ToString(),
                             resolveError);
                continue;
            }

            if (pendingRef.isRawPointer)
            {
                if (pendingRef.property != nullptr
                    && pendingRef.property->HasPropertySetter()
                    && pendingRef.ownerObjectPtr != nullptr)
                {
                    void* rawValue = resolvedRawPtr;
                    if (!AssignProperty(pendingRef.ownerObjectPtr, *pendingRef.property, &rawValue))
                    {
                        remainingRefs.push_back(pendingRef);
                        ME_CORE_WARN("Pending object reference AssignProperty failed. path='{}', guid='{}'",
                                     pendingRef.fieldPath,
                                     pendingRef.refGuid.ToString());
                        continue;
                    }
                }
                else
                {
                    *static_cast<void**>(pendingRef.ptrToPtr) = resolvedRawPtr;
                }
                ++resolvedCount;
                continue;
            }

            if (pendingRef.expectedClass == nullptr
                || !pendingRef.expectedClass->SetSharedPtr(resolvedSharedPtr, pendingRef.ptrToPtr))
            {
                remainingRefs.push_back(pendingRef);
                ME_CORE_WARN("Pending object reference assignment failed. path='{}', guid='{}'",
                             pendingRef.fieldPath,
                             pendingRef.refGuid.ToString());
                continue;
            }

            ++resolvedCount;
        }

        unresolvedRefs = std::move(remainingRefs);

        ME_CORE_INFO("Pending object reference resolve pass finished. resolved={}, unresolved={}",
                     resolvedCount,
                     unresolvedRefs.size());

        if (!unresolvedRefs.empty())
        {
            return SerializeResult::Failure("Resolve pending object references finished with unresolved entries.", "PendingObjectReferences");
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::ToFile(const std::string& filePath,
                                       const MEClass* rootClass,
                                       const void* rootObject,
                                       WriterArchive& archive,
                                       const SerializerOptions& options)
    {
        if (filePath.empty())
        {
            return SerializeResult::Failure("ToFile failed: filePath is empty.");
        }

        archive.ResetWriteState();

        SerializeResult serializeResult = Serialize(rootClass, rootObject, archive, options);
        if (!serializeResult.ok)
        {
            return serializeResult;
        }

        if (options.writeSchemaVersion)
        {
            if (auto* jsonWriter = dynamic_cast<JsonWriterArchive*>(&archive))
            {
                jsonWriter->ApplyRootSchemaVersion(options.schemaVersion);
            }
        }

        if (!archive.WriteToFile(filePath))
        {
            std::string message = "ToFile failed: archive write failed.";
            const std::string& archiveError = archive.GetLastArchiveError();
            if (!archiveError.empty())
            {
                message += " reason: " + archiveError;
            }

            return SerializeResult::Failure(message, filePath);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::ToFile(const std::string& filePath,
                                       const std::string& rootClassName,
                                       const void* rootObject,
                                       WriterArchive& archive,
                                       const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("ToFile failed: root class not found.", rootClassName);
        }

        return ToFile(filePath, rootClass, rootObject, archive, options);
    }

    SerializeResult Serializer::FromFile(const std::string& filePath,
                                         const MEClass* rootClass,
                                         void* outRootObject,
                                         ReaderArchive& archive,
                                         const SerializerOptions& options)
    {
        if (filePath.empty())
        {
            return SerializeResult::Failure("FromFile failed: filePath is empty.");
        }

        std::vector<PendingObjectRef> unresolvedRefs;
        archive.ResetReadState();

        if (!archive.ReadFromFile(filePath))
        {
            std::string message = "FromFile failed: archive read failed.";
            const std::string& archiveError = archive.GetLastArchiveError();
            if (!archiveError.empty())
            {
                message += " reason: " + archiveError;
            }

            return SerializeResult::Failure(message, filePath);
        }

        SerializeResult deserializeResult = Deserialize(rootClass, outRootObject, archive, unresolvedRefs, options);
        if (!deserializeResult.ok)
        {
            return deserializeResult;
        }

        return ResolvePendingObjectRefs(unresolvedRefs);
    }

    SerializeResult Serializer::FromFile(const std::string& filePath,
                                         const std::string& rootClassName,
                                         void* outRootObject,
                                         ReaderArchive& archive,
                                         const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("FromFile failed: root class not found.", rootClassName);
        }

        return FromFile(filePath, rootClass, outRootObject, archive, options);
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

        if (!archive.BeginObject(classInfo, options.writeObjectTypeName))
        {
            return SerializeResult::Failure("Serialize class failed: BeginObject returned false.", path);
        }

        // Iterate properties in the class hierarchy and serialize them
        SerializeResult iterationResult = SerializeObject_IterateProps(classInfo, objectPtr, archive, options, path);
        if (!iterationResult.ok)
        {
            return iterationResult;
        }

        if (!archive.EndObject())
        {
            return SerializeResult::Failure("Serialize class failed: EndObject returned false.", path);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::SerializeProperty(const MEProperty& property,
                                                  const Reflection::PropertySpecifierMask propertySpecifierMask,
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
            const auto* primitive = static_cast<const MEPrimitiveProperty*>(&property);
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
            const auto* objectProperty = static_cast<const MEObjectProperty*>(&property);
            const MEClass* valueClass = objectProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Serialize object failed: value class is unresolved.", path);
            }

            return SerializeObjectInstance(valueClass, valuePtr, archive, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            const auto* objectPtrProperty = static_cast<const MEObjectPtrProperty*>(&property);
            return SerializeObjectPtr(*objectPtrProperty, objectPtrProperty->GetSpecifierMask() | propertySpecifierMask, valuePtr, ownerObjectPtr, archive, options, path);

        }
        case MEPropertyCategory::Array:
        {
            const auto* arrayProperty = static_cast<const MEArrayProperty*>(&property);
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
                                                                  propertySpecifierMask,
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
                                                    const Reflection::PropertySpecifierMask propertySpecifierMask,
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

        const MEClass* meObjectClass = MEObject::StaticClass();
        // Make sure the value class is derived from MEObject.
        if (!staticValueClass->IsA(meObjectClass))
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
        const bool shouldSerializeInline = (objectPtr->GetOuter() == ownerObject && static_cast<int>(PropertySpecifier::Instanced) & propertySpecifierMask);

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

        if (!archive.BeginObjectPtr(dynamicClass))
        {
            return SerializeResult::Failure("Serialize class failed: BeginObjectPtr returned false.", path);
        }

        SerializeResult iterationResult = SerializeObject_IterateProps(dynamicClass, objectPtr, archive, options, path);
        if (!iterationResult.ok)
        {
            return iterationResult;
        }

        if (!archive.EndObjectPtr())
        {
            return SerializeResult::Failure("Serialize class failed: EndObjectPtr returned false.", path);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::SerializeObject_IterateProps(const minEngine::Reflection::MEClass *classInfo,
                                                             const void *objectPtr,
                                                             WriterArchive &archive,
                                                             const SerializerOptions &options,
                                                             const std::string &path)
    {
        SerializeResult result = SerializeResult::Success();
        const bool iterationOk = ReflectionSystem::Get().ForEachPropertyInHierarchy(
        classInfo,
        [&](const MEProperty& property) -> bool
        {
            if (property.HasSpecifier(PropertySpecifier::Transient))
            {
                return true;
            }

            if (property.GetConstAccessor() == nullptr)
            {
                result = SerializeResult::Failure("Serialize property failed: const accessor is null.", JoinPath(path, property.GetName()));
                return false;
            }

            const void* valuePtr = property.GetConst(objectPtr);
            if (valuePtr == nullptr)
            {
                result = SerializeResult::Failure("Serialize property failed: value pointer is null.", JoinPath(path, property.GetName()));
                return false;
            }

            if (!archive.BeginField(property.GetName()))
            {
                result = SerializeResult::Failure("Serialize property failed: BeginField returned false.", JoinPath(path, property.GetName()));
                return false;
            }

            // Pass ownerObjectPtr to SerializeProperty for potential use in serializing object pointer property
            result = SerializeProperty(property, property.GetSpecifierMask(), valuePtr, objectPtr, archive, options, JoinPath(path, property.GetName()));
            if (!result.ok)
            {
                return false;
            }

            if (!archive.EndField())
            {
                result = SerializeResult::Failure("Serialize property failed: EndField returned false.", JoinPath(path, property.GetName()));
                return false;
            }

            return true;
        });

        if (!iterationOk)
        {
            if (!result.ok)
            {
                return result;
            }

            return SerializeResult::Failure("Serialize class failed during property iteration.", path);
        }

        return SerializeResult::Success();
    }

    // Private methods for deserialization
    SerializeResult Serializer::DeserializeObjectInstance(const MEClass* classInfo,
                                                          void* objectPtr,
                                                          ReaderArchive& archive,
                                                          std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                          const SerializerOptions& options,
                                                          const std::string& path)
    {
        if (objectPtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize class failed: object pointer is null.", path);
        }

        const MEClass* meObjectClass = MEObject::StaticClass();
        if (meObjectClass != nullptr && ReflectionSystem::Get().IsClassSameOrDerived(classInfo, meObjectClass))
        {
            MEObject* meObject = static_cast<MEObject*>(objectPtr);
            if (meObject->GetClass() == nullptr)
            {
                meObject->SetClass(classInfo);
            }
        }

        if (!archive.BeginObject(classInfo))
        {
            std::string message = "Deserialize class failed: BeginObject returned false.";
            const std::string& archiveError = archive.GetLastArchiveError();
            if (!archiveError.empty())
            {
                message += " reason: " + archiveError;
            }

            return SerializeResult::Failure(message, path);
        }

        // Iterate the properties in hierarchy and deserialize each property.
        SerializeResult iterationResult = DeserializeObject_IterateProps(classInfo, objectPtr, archive, outUnresolvedRefs, options, path);
        if (!iterationResult.ok)
        {
            return iterationResult;
        }

        if (!archive.EndObject())
        {
            std::string message = "Deserialize class failed: EndObject returned false.";
            const std::string& archiveError = archive.GetLastArchiveError();
            if (!archiveError.empty())
            {
                message += " reason: " + archiveError;
            }
            return SerializeResult::Failure(message, path);
        }

        return SerializeResult::Success();
    }

    SerializeResult Serializer::DeserializeProperty(const MEProperty& property,
                                                          void* outValuePtr,
                                                          void* ownerObjectPtr,
                                                          ReaderArchive& archive,
                                                          std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                          const SerializerOptions& options,
                                                          const std::string& path)
    {
        switch (property.GetCategory())
        {
        case MEPropertyCategory::Primitive:
        {
            const auto* primitive = static_cast<const MEPrimitiveProperty*>(&property);
            const PrimitiveCodec* codec = PrimitiveCodecRegistry::Get().Find(primitive->primitiveTypeName);
            if (codec == nullptr)
            {
                return SerializeResult::Failure("Deserialize primitive failed: codec not found for type '" + primitive->primitiveTypeName + "'.", path);
            }

            if (!codec->read(archive, outValuePtr))
            {
                if (!options.strictTypeCheck)
                {
                    ME_CORE_WARN(
                        "Deserialize primitive skipped (type mismatch / codec fail). path='{}', type='{}'",
                        path,
                        primitive->primitiveTypeName);
                    return SerializeResult::Success();
                }

                return SerializeResult::Failure("Deserialize primitive failed: codec read returned false.", path);
            }

            return SerializeResult::Success();
        }
        case MEPropertyCategory::Object:
        {
            const auto* objectProperty = static_cast<const MEObjectProperty*>(&property);
            const MEClass* valueClass = objectProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Deserialize object failed: value class is unresolved.", path);
            }

            return DeserializeObjectInstance(valueClass, outValuePtr, archive, outUnresolvedRefs, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            const auto* objectPtrProperty = static_cast<const MEObjectPtrProperty*>(&property);
            const MEClass* valueClass = objectPtrProperty->GetValueClass();
            if (valueClass == nullptr)
            {
                return SerializeResult::Failure("Deserialize object pointer failed: value class is unresolved.", path);
            }

            // outValuePtr actually serves as a pointer to pointer for object pointer property, we need to dereference it first to get the current pointer value.
            return DeserializeObjectPtr(*objectPtrProperty, outValuePtr, ownerObjectPtr, archive, outUnresolvedRefs, options, path);

        }
        case MEPropertyCategory::Array:
        {
            const auto* arrayProperty = static_cast<const MEArrayProperty*>(&property);
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
                                                                    outUnresolvedRefs,
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
                                                                                std::vector<PendingObjectRef>& outUnresolvedRefs,
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

        const MEClass* meObjectClass = MEObject::StaticClass();
        const bool supportsMEObject = (meObjectClass != nullptr) && ReflectionSystem::Get().IsClassSameOrDerived(classInfo, meObjectClass);

        if (archive.ReadNull())
        {
            // Here is a good example of how we assign any value to the pointer.
            if (ptrCategory == MEObjectPtrCategory::Raw)
            {
                if (!WriteRawObjectPtrValue(ownerObjectPtr, objectPtrProperty, ptrToPtr, nullptr))
                {
                    return SerializeResult::Failure("Deserialize object pointer failed: failed to assign null raw pointer.", path);
                }
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

            // Create the object shell first and then deserialize properties into it.
            std::shared_ptr<void> newObjectPtr = dynamicClassInfo->CreateDefaultInstance();
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
            

            SerializeResult iterationResult = DeserializeObject_IterateProps(dynamicClassInfo, objectPtr, archive, outUnresolvedRefs, options, path);
            if (!iterationResult.ok)
            {
                (void)archive.EndObjectPtr();
                return iterationResult;
            }

            if (ptrCategory == MEObjectPtrCategory::Raw)
            {
                if (!supportsMEObject)
                {
                    const bool closed = archive.EndObjectPtr();
                    if (!closed)
                    {
                        return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false for unsupported raw pointer type.", path);
                    }
                    return SerializeResult::Failure("Deserialize object pointer failed: inline raw pointer requires MEObject-derived type.", path);
                }

                if (!WriteRawObjectPtrValue(ownerObjectPtr, objectPtrProperty, ptrToPtr, objectPtr))
                {
                    const bool closed = archive.EndObjectPtr();
                    if (!closed)
                    {
                        return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after raw pointer assign failure.", path);
                    }
                    return SerializeResult::Failure("Deserialize object pointer failed: failed to assign raw pointer.", path);
                }
            }
            else
            {
                if (!classInfo->SetSharedPtr(newObjectPtr, ptrToPtr))
                {
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

            // If everything goes well, register the new MEObject.
            std::shared_ptr<MEObject> managedObject;
            if (supportsMEObject)
            {
                MEObject* meObjectPtr = static_cast<MEObject*>(objectPtr);
                meObjectPtr->SetClass(dynamicClassInfo);
                meObjectPtr->SetOuter(static_cast<MEObject*>(ownerObjectPtr));

                managedObject = std::static_pointer_cast<MEObject>(newObjectPtr);

                if (SceneCloneContext* cloneContext = GetActiveCloneContext())
                {
                    const GUID sourceGuid = meObjectPtr->GetGuid();
                    const GUID newGuid = GenerateGUID();
                    cloneContext->RecordClone(sourceGuid, managedObject, newGuid);
                    meObjectPtr->SetGuid(newGuid);
                }

                ObjectManager::Get().RegisterObject(managedObject);
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
        pendingRef.property = &objectPtrProperty;
        pendingRef.isRawPointer = (ptrCategory == MEObjectPtrCategory::Raw);
        pendingRef.expectsMEObject = supportsMEObject;
        pendingRef.fieldPath = path;
        outUnresolvedRefs.push_back(std::move(pendingRef));

        if (ptrCategory == MEObjectPtrCategory::Raw)
        {
            // Clear storage without Setter — resolve will AssignProperty when Setter is present.
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

    SerializeResult Serializer::DeserializeObject_IterateProps(const minEngine::Reflection::MEClass* classInfo,
                                                               void* objectPtr,
                                                               ReaderArchive& archive,
                                                               std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                               const SerializerOptions& options,
                                                               const std::string& path)
    {
        SerializeResult result = SerializeResult::Success();
        const bool iterationOk = ReflectionSystem::Get().ForEachPropertyInHierarchy(
        classInfo,
        [&](const MEProperty& property) -> bool
        {
            if (property.HasSpecifier(PropertySpecifier::Transient))
            {
                return true;
            }

            const std::string propertyPath = JoinPath(path, property.GetName());
            const bool hasField = archive.EnterField(property.GetName());
            if (!hasField)
            {
                if (options.skipUnknownField)
                {
                    return true;
                }

                result = SerializeResult::Failure("Deserialize property failed: missing field.", propertyPath);
                return false;
            }

            if (property.GetMutableAccessor() == nullptr)
            {
                result = SerializeResult::Failure("Deserialize property failed: mutable accessor is null.", propertyPath);
                return false;
            }

            void* valuePtr = property.GetMutable(objectPtr);
            if (valuePtr == nullptr)
            {
                result = SerializeResult::Failure("Deserialize property failed: value pointer is null.", propertyPath);
                return false;
            }

            if (property.HasPropertySetter() && property.GetCategory() != MEPropertyCategory::ObjectPtr)
            {
                AlignedPropertyTemp temp;
                if (!temp.Allocate(property))
                {
                    result = SerializeResult::Failure(
                        "Deserialize property failed: failed to allocate temp storage for Setter.",
                        propertyPath);
                    return false;
                }

                result = DeserializeProperty(property, temp.ptr, objectPtr, archive, outUnresolvedRefs, options, propertyPath);
                if (result.ok && !AssignProperty(objectPtr, property, temp.ptr))
                {
                    result = SerializeResult::Failure("Deserialize property failed: AssignProperty returned false.", propertyPath);
                }

                temp.Destroy(property);
            }
            else
            {
                result = DeserializeProperty(property, valuePtr, objectPtr, archive, outUnresolvedRefs, options, propertyPath);
            }

            if (!result.ok)
            {
                return false;
            }

            const bool leaveOk = archive.LeaveField();
            if (!leaveOk)
            {
                result = SerializeResult::Failure("Deserialize property failed: LeaveField returned false.", propertyPath);
                return false;
            }

            return result.ok;
        });

        if (!iterationOk)
        {
            if (!result.ok)
            {
                return result;
            }

            return SerializeResult::Failure("Deserialize class failed during property iteration.", path);
        }

        return SerializeResult::Success();
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

        if (SceneCloneContext* cloneContext = GetActiveCloneContext())
        {
            std::shared_ptr<MEObject> clonedObject = cloneContext->ResolveSceneRefShared(pendingRef.refGuid);
            if (clonedObject != nullptr)
            {
                if (pendingRef.expectsMEObject && pendingRef.expectedClass != nullptr)
                {
                    const MEClass* trackedClass = clonedObject->GetClass();
                    if (trackedClass != nullptr
                        && !ReflectionSystem::Get().IsClassSameOrDerived(trackedClass, pendingRef.expectedClass))
                    {
                        outErrorMessage = "resolved cloned object type mismatch";
                        return false;
                    }
                }

                outResolvedSharedPtr = std::static_pointer_cast<void>(clonedObject);
                outResolvedRawPtr = clonedObject.get();
                return true;
            }

            return ResolvePendingAssetRef(pendingRef, outResolvedSharedPtr, outResolvedRawPtr, outErrorMessage);
        }

        // First try to find the referenced object in the object manager using the GUID.
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

        // Then try to find the referenced asset in the asset registry using the GUID and load it.
        return ResolvePendingAssetRef(pendingRef, outResolvedSharedPtr, outResolvedRawPtr, outErrorMessage);
    }

    bool Serializer::ResolvePendingAssetRef(const PendingObjectRef &pendingRef, std::shared_ptr<void> &outResolvedSharedPtr, void *&outResolvedRawPtr, std::string &outErrorMessage)
    {
        outResolvedSharedPtr = std::static_pointer_cast<void>(AssetManager::Get().LoadAssetByGUID(pendingRef.refGuid, outErrorMessage));
        outResolvedRawPtr = outResolvedSharedPtr.get();

        if (outResolvedSharedPtr == nullptr)
        {
            if (outErrorMessage.empty())
            {
                outErrorMessage = "asset resolve returned null";
            }
            return false;
        }

        return true;
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

    const MEProperty* Serializer::FindPropertyInHierarchy(const MEClass* ownerClass, std::string_view propertyName)
    {
        return FindPropertyInHierarchyShared(ownerClass, propertyName);
    }

    SerializeResult Serializer::SerializeProperty(void* ownerObject,
                                                  const MEClass* ownerClass,
                                                  const std::string& propertyName,
                                                  WriterArchive& archive,
                                                  const SerializerOptions& options)
    {
        if (ownerObject == nullptr)
        {
            return SerializeResult::Failure("Serialize property failed: ownerObject is null.", propertyName);
        }

        if (ownerClass == nullptr)
        {
            return SerializeResult::Failure("Serialize property failed: ownerClass is null.", propertyName);
        }

        const MEProperty* property = FindPropertyInHierarchy(ownerClass, propertyName);
        if (property == nullptr)
        {
            return SerializeResult::Failure("Serialize property failed: property not found.", propertyName);
        }

        if (property->GetConstAccessor() == nullptr)
        {
            return SerializeResult::Failure("Serialize property failed: const accessor is null.", propertyName);
        }

        const void* valuePtr = property->GetConst(ownerObject);
        if (valuePtr == nullptr)
        {
            return SerializeResult::Failure("Serialize property failed: value pointer is null.", propertyName);
        }

        return SerializeProperty(*property,
                                 property->GetSpecifierMask(),
                                 valuePtr,
                                 ownerObject,
                                 archive,
                                 options,
                                 propertyName);
    }

    SerializeResult Serializer::DeserializeProperty(void* ownerObject,
                                                    const MEClass* ownerClass,
                                                    const std::string& propertyName,
                                                    ReaderArchive& archive,
                                                    std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                    const SerializerOptions& options)
    {
        if (ownerObject == nullptr)
        {
            return SerializeResult::Failure("Deserialize property failed: ownerObject is null.", propertyName);
        }

        if (ownerClass == nullptr)
        {
            return SerializeResult::Failure("Deserialize property failed: ownerClass is null.", propertyName);
        }

        const MEProperty* property = FindPropertyInHierarchy(ownerClass, propertyName);
        if (property == nullptr)
        {
            return SerializeResult::Failure("Deserialize property failed: property not found.", propertyName);
        }

        if (property->GetMutableAccessor() == nullptr)
        {
            return SerializeResult::Failure("Deserialize property failed: mutable accessor is null.", propertyName);
        }

        void* valuePtr = property->GetMutable(ownerObject);
        if (valuePtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize property failed: value pointer is null.", propertyName);
        }

        if (property->HasPropertySetter() && property->GetCategory() != MEPropertyCategory::ObjectPtr)
        {
            AlignedPropertyTemp temp;
            if (!temp.Allocate(*property))
            {
                return SerializeResult::Failure(
                    "Deserialize property failed: failed to allocate temp storage for Setter.",
                    propertyName);
            }

            SerializeResult result = DeserializeProperty(
                *property,
                temp.ptr,
                ownerObject,
                archive,
                outUnresolvedRefs,
                options,
                propertyName);
            if (result.ok && !AssignProperty(ownerObject, *property, temp.ptr))
            {
                temp.Destroy(*property);
                return SerializeResult::Failure("Deserialize property failed: AssignProperty returned false.", propertyName);
            }

            temp.Destroy(*property);
            return result;
        }

        return DeserializeProperty(*property,
                                   valuePtr,
                                   ownerObject,
                                   archive,
                                   outUnresolvedRefs,
                                   options,
                                   propertyName);
    }

    SerializeResult Serializer::SerializePropertyToBuffer(void* ownerObject,
                                                          const MEClass* ownerClass,
                                                          const std::string& propertyName,
                                                          std::vector<uint8_t>& outBuffer,
                                                          const SerializerOptions& options)
    {
        SerializerOptions binaryOptions = options;
        binaryOptions.skipUnknownField = false;
        binaryOptions.strictTypeCheck = true;

        BinaryWriterArchive writer;
        SerializeResult result = SerializeProperty(ownerObject, ownerClass, propertyName, writer, binaryOptions);
        if (!result.ok)
        {
            return result;
        }

        outBuffer = writer.TakeBuffer();
        return SerializeResult::Success();
    }

    SerializeResult Serializer::DeserializePropertyFromBuffer(void* ownerObject,
                                                              const MEClass* ownerClass,
                                                              const std::string& propertyName,
                                                              const std::vector<uint8_t>& buffer,
                                                              std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                              const SerializerOptions& options)
    {
        SerializerOptions binaryOptions = options;
        binaryOptions.skipUnknownField = false;
        binaryOptions.strictTypeCheck = true;

        BinaryReaderArchive reader(buffer);
        return DeserializeProperty(ownerObject, ownerClass, propertyName, reader, outUnresolvedRefs, binaryOptions);
    }

    SerializeResult Serializer::SerializePropertyByPathToBuffer(void* ownerObject,
                                                               const MEClass* ownerClass,
                                                               const std::string& propertyPath,
                                                               std::vector<uint8_t>& outBuffer,
                                                               const SerializerOptions& options)
    {
        std::vector<std::string_view> segments;
        if (!SplitPropertyPath(propertyPath, segments))
        {
            return SerializeResult::Failure("SerializePropertyByPathToBuffer failed: invalid propertyPath.", propertyPath);
        }

        void* currentOwnerObject = ownerObject;
        const MEClass* currentOwnerClass = ownerClass;
        SerializeResult walkResult =
            WalkToOwningObjectByPath(currentOwnerObject, currentOwnerClass, segments, propertyPath, false);
        if (!walkResult.ok)
        {
            return walkResult;
        }

        const std::string leafName(segments.back());
        return SerializePropertyToBuffer(currentOwnerObject, currentOwnerClass, leafName, outBuffer, options);
    }

    SerializeResult Serializer::DeserializePropertyByPathFromBuffer(void* ownerObject,
                                                                   const MEClass* ownerClass,
                                                                   const std::string& propertyPath,
                                                                   const std::vector<uint8_t>& buffer,
                                                                   std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                                   const SerializerOptions& options)
    {
        std::vector<std::string_view> segments;
        if (!SplitPropertyPath(propertyPath, segments))
        {
            return SerializeResult::Failure("DeserializePropertyByPathFromBuffer failed: invalid propertyPath.", propertyPath);
        }

        void* currentOwnerObject = ownerObject;
        const MEClass* currentOwnerClass = ownerClass;
        SerializeResult walkResult =
            WalkToOwningObjectByPath(currentOwnerObject, currentOwnerClass, segments, propertyPath, true);
        if (!walkResult.ok)
        {
            return walkResult;
        }

        const std::string leafName(segments.back());
        return DeserializePropertyFromBuffer(
            currentOwnerObject,
            currentOwnerClass,
            leafName,
            buffer,
            outUnresolvedRefs,
            options);
    }

    SerializeResult Serializer::SerializeObjectToBuffer(const MEClass* rootClass,
                                                        const void* rootObject,
                                                        std::vector<uint8_t>& outBuffer,
                                                        const SerializerOptions& options)
    {
        SerializerOptions binaryOptions = options;
        binaryOptions.skipUnknownField = false;
        binaryOptions.strictTypeCheck = true;

        BinaryWriterArchive writer;
        SerializeResult result = Serialize(rootClass, rootObject, writer, binaryOptions);
        if (!result.ok)
        {
            return result;
        }

        outBuffer = writer.TakeBuffer();
        return SerializeResult::Success();
    }

    SerializeResult Serializer::SerializeObjectToBuffer(const std::string& rootClassName,
                                                        const void* rootObject,
                                                        std::vector<uint8_t>& outBuffer,
                                                        const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("SerializeObjectToBuffer failed: root class not found.", rootClassName);
        }

        return SerializeObjectToBuffer(rootClass, rootObject, outBuffer, options);
    }

    SerializeResult Serializer::DeserializeObjectFromBuffer(const MEClass* rootClass,
                                                            void* outRootObject,
                                                            const std::vector<uint8_t>& buffer,
                                                            std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                            const SerializerOptions& options)
    {
        SerializerOptions binaryOptions = options;
        binaryOptions.skipUnknownField = false;
        binaryOptions.strictTypeCheck = true;

        BinaryReaderArchive reader(buffer);
        return Deserialize(rootClass, outRootObject, reader, outUnresolvedRefs, binaryOptions);
    }

    SerializeResult Serializer::DeserializeObjectFromBuffer(const std::string& rootClassName,
                                                            void* outRootObject,
                                                            const std::vector<uint8_t>& buffer,
                                                            std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                            const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("DeserializeObjectFromBuffer failed: root class not found.", rootClassName);
        }

        return DeserializeObjectFromBuffer(rootClass, outRootObject, buffer, outUnresolvedRefs, options);
    }

    SerializeResult Serializer::SerializeObjectToJson(const MEClass* rootClass,
                                                      const void* rootObject,
                                                      Json& outRoot,
                                                      const SerializerOptions& options)
    {
        JsonWriterArchive writer;
        const SerializeResult result = Serialize(rootClass, rootObject, writer, options);
        if (!result.ok)
        {
            return result;
        }

        if (options.writeSchemaVersion)
        {
            writer.ApplyRootSchemaVersion(options.schemaVersion);
        }

        outRoot = std::move(writer.MoveRoot());
        return SerializeResult::Success();
    }

    SerializeResult Serializer::SerializeObjectToJson(const std::string& rootClassName,
                                                      const void* rootObject,
                                                      Json& outRoot,
                                                      const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("SerializeObjectToJson failed: root class not found.", rootClassName);
        }

        return SerializeObjectToJson(rootClass, rootObject, outRoot, options);
    }

    SerializeResult Serializer::DeserializeObjectFromJson(const MEClass* rootClass,
                                                          void* outRootObject,
                                                          const Json& root,
                                                          std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                          const SerializerOptions& options)
    {
        JsonReaderArchive reader(root);
        return Deserialize(rootClass, outRootObject, reader, outUnresolvedRefs, options);
    }

    SerializeResult Serializer::DeserializeObjectFromJson(const std::string& rootClassName,
                                                          void* outRootObject,
                                                          const Json& root,
                                                          std::vector<PendingObjectRef>& outUnresolvedRefs,
                                                          const SerializerOptions& options)
    {
        const MEClass* rootClass = ReflectionSystem::Get().FindClass(rootClassName);
        if (rootClass == nullptr)
        {
            return SerializeResult::Failure("DeserializeObjectFromJson failed: root class not found.", rootClassName);
        }

        return DeserializeObjectFromJson(rootClass, outRootObject, root, outUnresolvedRefs, options);
    }
}
