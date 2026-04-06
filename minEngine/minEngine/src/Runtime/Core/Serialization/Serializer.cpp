#include "Serializer.h"

#include <fstream>
#include <unordered_set>

namespace minEngine
{
    template<typename TValue>
    Serializer::Json Serializer::Write(const TValue&)
    {
        static_assert(!std::is_same_v<TValue, TValue>, "Serializer::Write<T> is not specialized for this type.");
        return Json::object();
    }

    template<typename TValue>
    bool Serializer::Read(const Json&, TValue&)
    {
        static_assert(!std::is_same_v<TValue, TValue>, "Serializer::Read<T> is not specialized for this type.");
        return false;
    }

    template<>
    Serializer::Json Serializer::Write<int>(const int& value)
    {
        return Json(value);
    }

    template<>
    Serializer::Json Serializer::Write<float>(const float& value)
    {
        return Json(value);
    }

    template<>
    Serializer::Json Serializer::Write<double>(const double& value)
    {
        return Json(value);
    }

    template<>
    Serializer::Json Serializer::Write<bool>(const bool& value)
    {
        return Json(value);
    }

    template<>
    Serializer::Json Serializer::Write<std::string>(const std::string& value)
    {
        return Json(value);
    }

    template<>
    Serializer::Json Serializer::Write<Vector2>(const Vector2& value)
    {
        return Json::array({value.x, value.y});
    }

    template<>
    Serializer::Json Serializer::Write<Vector3>(const Vector3& value)
    {
        return Json::array({value.x, value.y, value.z});
    }

    template<>
    Serializer::Json Serializer::Write<Vector4>(const Vector4& value)
    {
        return Json::array({value.x, value.y, value.z, value.w});
    }

    template<>
    bool Serializer::Read<int>(const Json& json, int& outValue)
    {
        if (!json.is_number_integer())
        {
            return false;
        }
        outValue = json.get<int>();
        return true;
    }

    template<>
    bool Serializer::Read<float>(const Json& json, float& outValue)
    {
        if (!json.is_number())
        {
            return false;
        }
        outValue = json.get<float>();
        return true;
    }

    template<>
    bool Serializer::Read<double>(const Json& json, double& outValue)
    {
        if (!json.is_number())
        {
            return false;
        }
        outValue = json.get<double>();
        return true;
    }

    template<>
    bool Serializer::Read<bool>(const Json& json, bool& outValue)
    {
        if (!json.is_boolean())
        {
            return false;
        }
        outValue = json.get<bool>();
        return true;
    }

    template<>
    bool Serializer::Read<std::string>(const Json& json, std::string& outValue)
    {
        if (!json.is_string())
        {
            return false;
        }
        outValue = json.get<std::string>();
        return true;
    }

    template<>
    bool Serializer::Read<Vector2>(const Json& json, Vector2& outValue)
    {
        if (!json.is_array() || json.size() != 2)
        {
            return false;
        }
        outValue.x = json[0].get<float>();
        outValue.y = json[1].get<float>();
        return true;
    }

    template<>
    bool Serializer::Read<Vector3>(const Json& json, Vector3& outValue)
    {
        if (!json.is_array() || json.size() != 3)
        {
            return false;
        }
        outValue.x = json[0].get<float>();
        outValue.y = json[1].get<float>();
        outValue.z = json[2].get<float>();
        return true;
    }

    template<>
    bool Serializer::Read<Vector4>(const Json& json, Vector4& outValue)
    {
        if (!json.is_array() || json.size() != 4)
        {
            return false;
        }
        outValue.x = json[0].get<float>();
        outValue.y = json[1].get<float>();
        outValue.z = json[2].get<float>();
        outValue.w = json[3].get<float>();
        return true;
    }

    Serializer::Json Serializer::ToJsonByTypeInfo(const void* object, const Reflection::TypeInfo& typeInfo)
    {
        Json result = Json::object();
        result["__type"] = typeInfo.name;

        Json fieldsByType = Json::object();
        const Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        reflectionSystem.ForEachFieldInHierarchy(typeInfo.name,
            [&](const Reflection::TypeInfo& declaringType, const Reflection::FieldInfo& field)
            {
                const void* declaredObjectPtr = reflectionSystem.CastObjectToType(object, typeInfo.name, declaringType.name);
                if (declaredObjectPtr == nullptr)
                {
                    ME_CORE_WARN("[Serializer] Failed to cast '{}' object to declaring type '{}' for field '{}'.",
                                 typeInfo.name,
                                 declaringType.name,
                                 field.name);
                    return true;
                }

                const void* fieldPtr = Reflection::ReflectionSystem::GetFieldPtr(declaredObjectPtr, field);
                if (fieldPtr == nullptr)
                {
                    ME_CORE_WARN("[Serializer] Field pointer is null for '{}::{}'", declaringType.name, field.name);
                    return true;
                }

                Json fieldValue;
                if (!WriteFieldValue(field, fieldPtr, fieldValue))
                {
                    ME_CORE_WARN("[Serializer] Unsupported field type '{}' on field '{}::{}'",
                                 field.typeName,
                                 declaringType.name,
                                 field.name);
                    return true;
                }

                fieldsByType[declaringType.name][field.name] = std::move(fieldValue);
                return true;
            });

        result["fieldsByType"] = std::move(fieldsByType);

        return result;
    }

    bool Serializer::FromJsonByTypeInfo(const Json& json, void* object, const Reflection::TypeInfo& typeInfo)
    {
        if (!json.is_object())
        {
            return false;
        }

        const Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        const bool hasFieldsByType = json.contains("fieldsByType") && json["fieldsByType"].is_object();
        std::unordered_set<std::string> seenLegacyFieldNames;

        bool success = true;
        reflectionSystem.ForEachFieldInHierarchy(typeInfo.name,
            [&](const Reflection::TypeInfo& declaringType, const Reflection::FieldInfo& field)
            {
                void* declaredObjectPtr = reflectionSystem.CastObjectToType(object, typeInfo.name, declaringType.name);
                if (declaredObjectPtr == nullptr)
                {
                    ME_CORE_WARN("[Serializer] Failed to cast '{}' object to declaring type '{}' for field '{}'.",
                                 typeInfo.name,
                                 declaringType.name,
                                 field.name);
                    success = false;
                    return false;
                }

                void* fieldPtr = Reflection::ReflectionSystem::GetFieldPtr(declaredObjectPtr, field);
                if (fieldPtr == nullptr)
                {
                    ME_CORE_WARN("[Serializer] Field pointer is null for '{}::{}'", declaringType.name, field.name);
                    success = false;
                    return false;
                }

                if (hasFieldsByType)
                {
                    const Json& fieldsByType = json["fieldsByType"];
                    if (!fieldsByType.contains(declaringType.name) || !fieldsByType[declaringType.name].is_object())
                    {
                        return true;
                    }

                    const Json& typeBucket = fieldsByType[declaringType.name];
                    if (!typeBucket.contains(field.name))
                    {
                        return true;
                    }

                    if (!ReadFieldValue(field, typeBucket[field.name], fieldPtr))
                    {
                        ME_CORE_WARN("[Serializer] Failed to read field '{}::{}' with type '{}'",
                                     declaringType.name,
                                     field.name,
                                     field.typeName);
                        success = false;
                        return false;
                    }

                    return true;
                }

                // Backward compatibility for legacy flat format.
                if (!json.contains(field.name))
                {
                    return true;
                }

                if (!seenLegacyFieldNames.insert(field.name).second)
                {
                    ME_CORE_WARN("[Serializer] Legacy flat json field '{}' is ambiguous across inheritance hierarchy; skip duplicate.", field.name);
                    return true;
                }

                if (!ReadFieldValue(field, json[field.name], fieldPtr))
                {
                    ME_CORE_WARN("[Serializer] Failed to read legacy field '{}' with type '{}'", field.name, field.typeName);
                    success = false;
                    return false;
                }

                return true;
            });

        if (!success)
        {
            return false;
        }

        return true;
    }

    bool Serializer::SaveJsonToFile(const Json& json, const std::filesystem::path& filePath)
    {
        std::ofstream file(filePath, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            ME_CORE_ERROR("[Serializer] Failed to open file for write: {}", filePath.string());
            return false;
        }

        file << json.dump(4);
        file.flush();
        return file.good();
    }

    bool Serializer::LoadJsonFromFile(const std::filesystem::path& filePath, Json& outJson)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            ME_CORE_ERROR("[Serializer] Failed to open file for read: {}", filePath.string());
            return false;
        }

        try
        {
            file >> outJson;
        }
        catch (const std::exception& e)
        {
            ME_CORE_ERROR("[Serializer] Failed to parse json file '{}': {}", filePath.string(), e.what());
            return false;
        }

        return true;
    }

    bool Serializer::WriteFieldValue(const Reflection::FieldInfo& field, const void* fieldPtr, Json& outValue)
    {
        if (field.typeName == "int")
        {
            outValue = Write(*static_cast<const int*>(fieldPtr));
            return true;
        }
        if (field.typeName == "float")
        {
            outValue = Write(*static_cast<const float*>(fieldPtr));
            return true;
        }
        if (field.typeName == "double")
        {
            outValue = Write(*static_cast<const double*>(fieldPtr));
            return true;
        }
        if (field.typeName == "bool")
        {
            outValue = Write(*static_cast<const bool*>(fieldPtr));
            return true;
        }
        if (field.typeName == "std::string")
        {
            outValue = Write(*static_cast<const std::string*>(fieldPtr));
            return true;
        }
        if (field.typeName == "Vector2")
        {
            outValue = Write(*static_cast<const Vector2*>(fieldPtr));
            return true;
        }
        if (field.typeName == "Vector3")
        {
            outValue = Write(*static_cast<const Vector3*>(fieldPtr));
            return true;
        }
        if (field.typeName == "Vector4")
        {
            outValue = Write(*static_cast<const Vector4*>(fieldPtr));
            return true;
        }

        return false;
    }

    bool Serializer::ReadFieldValue(const Reflection::FieldInfo& field, const Json& value, void* fieldPtr)
    {
        if (field.typeName == "int")
        {
            return Read(value, *static_cast<int*>(fieldPtr));
        }
        if (field.typeName == "float")
        {
            return Read(value, *static_cast<float*>(fieldPtr));
        }
        if (field.typeName == "double")
        {
            return Read(value, *static_cast<double*>(fieldPtr));
        }
        if (field.typeName == "bool")
        {
            return Read(value, *static_cast<bool*>(fieldPtr));
        }
        if (field.typeName == "std::string")
        {
            return Read(value, *static_cast<std::string*>(fieldPtr));
        }
        if (field.typeName == "Vector2")
        {
            return Read(value, *static_cast<Vector2*>(fieldPtr));
        }
        if (field.typeName == "Vector3")
        {
            return Read(value, *static_cast<Vector3*>(fieldPtr));
        }
        if (field.typeName == "Vector4")
        {
            return Read(value, *static_cast<Vector4*>(fieldPtr));
        }

        return false;
    }
}
