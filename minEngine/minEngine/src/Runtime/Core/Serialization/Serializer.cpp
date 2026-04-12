#include "Serializer.h"

#include "PrimitiveCodecRegistry.h"
#include "Runtime/Core/Reflection/Reflection.h"

namespace minEngine::Serialization
{
    using minEngine::Reflection::MEArrayProperty;
    using minEngine::Reflection::MEClass;
    using minEngine::Reflection::MEObjectProperty;
    using minEngine::Reflection::MEPrimitiveProperty;
    using minEngine::Reflection::MEProperty;
    using minEngine::Reflection::MEPropertyCategory;
    using minEngine::Reflection::ReflectionSystem;

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

        return SerializeObject(rootClass, rootObject, archive, options, rootClassName);
    }

    SerializeResult Serializer::Deserialize(const std::string& rootClassName,
                                                     ReaderArchive& archive,
                                                     void* outRootObject,
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

        return DeserializeObject(rootClass, archive, outRootObject, options, rootClassName);
    }

    SerializeResult Serializer::SerializeObject(const MEClass* classInfo,
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

        SerializeResult result = SerializeResult::Success();
        const bool iterationOk = ReflectionSystem::Get().ForEachPropertyInHierarchy(
            classInfo->GetName(),
            [&](const MEProperty& property) -> bool
            {
                if (property.constAccessor == nullptr)
                {
                    result = SerializeResult::Failure("Serialize property failed: const accessor is null.", JoinPath(path, property.name));
                    return false;
                }

                const void* valuePtr = property.constAccessor(objectPtr);
                if (valuePtr == nullptr)
                {
                    result = SerializeResult::Failure("Serialize property failed: value pointer is null.", JoinPath(path, property.name));
                    return false;
                }

                if (!archive.BeginField(property.name))
                {
                    result = SerializeResult::Failure("Serialize property failed: BeginField returned false.", JoinPath(path, property.name));
                    return false;
                }

                result = SerializeProperty(property, valuePtr, archive, options, JoinPath(path, property.name));
                if (!result.ok)
                {
                    return false;
                }

                if (!archive.EndField())
                {
                    result = SerializeResult::Failure("Serialize property failed: EndField returned false.", JoinPath(path, property.name));
                    return false;
                }

                return true;
            });

        if (!iterationOk)
        {
            if (!archive.EndObject())
            {
                return SerializeResult::Failure("Serialize class failed after property error: EndObject returned false.", path);
            }
            return result.ok ? SerializeResult::Failure("Serialize class failed during property iteration.", path) : result;
        }

        if (!archive.EndObject())
        {
            return SerializeResult::Failure("Serialize class failed: EndObject returned false.", path);
        }

        return result;
    }

    SerializeResult Serializer::SerializeProperty(const MEProperty& property,
                                                           const void* valuePtr,
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

            return SerializeObject(valueClass, valuePtr, archive, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            // TODO: support object pointer later
            if (!options.allowObjectPtrSerialization)
            {
                return SerializeResult::Failure("Serialize object pointer failed: object pointer serialization is disabled in MVP.", path);
            }

            return archive.WriteNull()
                ? SerializeResult::Success()
                : SerializeResult::Failure("Serialize object pointer failed: WriteNull returned false.", path);
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

    SerializeResult Serializer::DeserializeObject(const MEClass* classInfo,
                                                          ReaderArchive& archive,
                                                          void* objectPtr,
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

        SerializeResult result = SerializeResult::Success();
        const bool iterationOk = ReflectionSystem::Get().ForEachPropertyInHierarchy(
            classInfo->GetName(),
            [&](const MEProperty& property) -> bool
            {
                const std::string propertyPath = JoinPath(path, property.name);
                const bool hasField = archive.EnterField(property.name);
                if (!hasField)
                {
                    if (options.skipUnknownField)
                    {
                        return true;
                    }

                    result = SerializeResult::Failure("Deserialize property failed: missing field.", propertyPath);
                    return false;
                }

                if (property.mutableAccessor == nullptr)
                {
                    result = SerializeResult::Failure("Deserialize property failed: mutable accessor is null.", propertyPath);
                    return false;
                }

                void* valuePtr = property.mutableAccessor(objectPtr);
                if (valuePtr == nullptr)
                {
                    result = SerializeResult::Failure("Deserialize property failed: value pointer is null.", propertyPath);
                    return false;
                }

                result = DeserializeProperty(property, archive, valuePtr, options, propertyPath);
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
            if (!archive.EndObject())
            {
                return SerializeResult::Failure("Deserialize class failed after property error: EndObject returned false.", path);
            }
            return result.ok ? SerializeResult::Failure("Deserialize class failed during property iteration.", path) : result;
        }

        if (!archive.EndObject())
        {
            return SerializeResult::Failure("Deserialize class failed: EndObject returned false.", path);
        }

        return result;
    }

    SerializeResult Serializer::DeserializeProperty(const MEProperty& property,
                                                             ReaderArchive& archive,
                                                             void* outValuePtr,
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

            return DeserializeObject(valueClass, archive, outValuePtr, options, path);
        }
        case MEPropertyCategory::ObjectPtr:
        {
            // TODO: support object pointer later
            if (!options.allowObjectPtrSerialization)
            {
                return SerializeResult::Failure("Deserialize object pointer failed: object pointer serialization is disabled in MVP.", path);
            }

            if (!archive.ReadNull())
            {
                return SerializeResult::Failure("Deserialize object pointer failed: only null is supported in MVP.", path);
            }

            return SerializeResult::Success();
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
                                                                    archive,
                                                                    elementPtr,
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
