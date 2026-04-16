#include "Serializer.h"

#include "PrimitiveCodecRegistry.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Object/MEObject.h"

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
        if(ownerObjectPtr == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: owner object pointer is null.", path);
        }

        MEObjectPtrCategory ptrCategory = objectPtrProperty.GetPtrCategory();
        if(ptrCategory == MEObjectPtrCategory::Invalid)
        {
            return SerializeResult::Failure("Serialize object pointer failed: invalid pointer category.", path);
        }

        // The original ptr must be a pointer of MEObject
        MEObject* objectPtr = static_cast<MEObject*>(objectPtrProperty.GetMutablePointingData(const_cast<void*>(ptrToPtr)));

        // Handle null pointer case first.
        // Write null to archive for nullptr object pointer.
        if(objectPtr == nullptr)
        {
            // Write null to archive for nullptr object pointer.
            if (!archive.WriteNull())
            {
                return SerializeResult::Failure("Serialize object pointer failed: WriteNull returned false.", path);
            }
            return SerializeResult::Success();
        }

        if(ptrCategory == MEObjectPtrCategory::Raw)
        {
            // Reference-semantic pointer serialization is not implemented yet.
            return SerializeResult::Failure("Serialize object pointer failed: serializing raw pointer property is not supported for inline value semantic.", path);
        }

        // Then handle non-null pointer case. 
        // We need to get the class info from the pointed object instance instead of the property, because the property only tells us the static type of the pointer (e.g. base class).
        // But the actual pointed object instance may be of a derived class, so we need to get the class info from the actual pointed object instance to ensure we serialize all the properties in the class hierarchy correctly.
        // For example, for a shared_pointer<Component> which actually points to a SceneComponent instance, we need to get the class info from the SceneComponent instance instead of the Component class, so that we can serialize the properties defined in SceneComponent class as well.
        const MEClass* classInfo = objectPtr->GetClass();
        if(classInfo == nullptr)
        {
            return SerializeResult::Failure("Serialize object pointer failed: value class is unresolved.", path);
        }

        // TODO: currently we only support serializing object pointer with "inline value" semantic
        // For "reference" semantic, we will need a more complex solution to handle object identity and references, which may require changes in the archive interface to allow recording unresolved references during serialization and resolving them later during deserialization.
        // So for now we will just return failure for "reference" semantic if the pointer category is raw pointer, until we have a complete solution for handling object references in place.
        if (!archive.BeginObjectPtr(classInfo->GetName()))
        {
            return SerializeResult::Failure("Serialize class failed: BeginObjectPtr returned false.", path);
        }

        // Iterate the properties in hierarchy and deserialize each property.
        const bool iterationOk = SerializeObject_IterateProps(classInfo, objectPtr, archive, options, path);
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
            return DeserializeObjectPtr(*objectPtrProperty, outValuePtr, archive, options, path);

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
                                                                                ReaderArchive &archive, 
                                                                                const SerializerOptions &options, 
                                                                                const std::string &path)
    {
        if (ptrToPtr == nullptr)
        {
            return SerializeResult::Failure("Deserialize class failed: object pointer is null.", path);
        }
        
        const MEClass* classInfo = objectPtrProperty.GetValueClass();
        if(classInfo == nullptr)
        {
            return SerializeResult::Failure("Deserialize object pointer failed: value class is unresolved.", path);
        }

        MEObjectPtrCategory ptrCategory = objectPtrProperty.GetPtrCategory();
        if(ptrCategory == MEObjectPtrCategory::Invalid)
        {
            return SerializeResult::Failure("Deserialize object pointer failed: invalid pointer category.", path);
        }

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

        if(ptrCategory == MEObjectPtrCategory::Raw)
        {
            // Reference-semantic pointer deserialization is not implemented yet.
            return SerializeResult::Failure("Deserialize object pointer failed: deserializing raw pointer property is not supported for inline value semantic.", path);
        }

        std::string dynamicClassName;
        if (!archive.BeginObjectPtr(classInfo, dynamicClassName))
        {
            return SerializeResult::Failure("Deserialize class failed: BeginObjectPtr returned false.", path);
        }

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

        // Iterate the properties in hierarchy and deserialize each property.
        const bool iterationOk = DeserializeObject_IterateProps(dynamicClassInfo, objectPtr, archive, options, path);

        if (!iterationOk)
        {
            const bool closed = archive.EndObjectPtr();
            if (!closed)
            {
                return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after property iteration failure.", path);
            }
            return SerializeResult::Failure("Deserialize class failed during property iteration.", path);
        }

        // Fill the shared pointer with the new object pointer we just created after we deserialized the object data into it.
        // This prevents poluting the shared pointer with a partially deserialized object in case some error happens during the deserialization of the object data.
        if (!classInfo->SetSharedPtr(newObjectPtr, ptrToPtr))
        {
            const bool closed = archive.EndObjectPtr();
            if (!closed)
            {
                return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false after assigning shared pointer.", path);
            }
            return SerializeResult::Failure("Deserialize object pointer failed: failed to assign shared pointer.", path);
        }


        if (!archive.EndObjectPtr())
        {
            return SerializeResult::Failure("Deserialize class failed: EndObjectPtr returned false.", path);
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

            result = DeserializeProperty(property, valuePtr, archive, options, propertyPath);
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
