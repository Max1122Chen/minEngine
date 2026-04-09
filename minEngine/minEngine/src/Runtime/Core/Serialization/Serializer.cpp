#include "Serializer.h"


#include <fstream>

namespace minEngine
{
    template<>
    Json Serializer::Write<int>(const int& value)
    {
        return Json(value);
    }

    template<>
    Json Serializer::Write<float>(const float& value)
    {
        return Json(value);
    }

    template<>
    Json Serializer::Write<double>(const double& value)
    {
        return Json(value);
    }

    template<>
    Json Serializer::Write<bool>(const bool& value)
    {
        return Json(value);
    }

    template<>
    Json Serializer::Write<std::string>(const std::string& value)
    {
        return Json(value);
    }

    template<>
    Json Serializer::Write<minEngine::Vector2>(const minEngine::Vector2& value)
    {
        return Json::array({value.x, value.y});
    }

    template<>
    Json Serializer::Write<minEngine::Vector3>(const minEngine::Vector3& value)
    {
        return Json::array({value.x, value.y, value.z});
    }

    template<>
    Json Serializer::Write<minEngine::Vector4>(const minEngine::Vector4& value)
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
    bool Serializer::Read<minEngine::Vector2>(const Json& json, minEngine::Vector2& outValue)
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
    bool Serializer::Read<minEngine::Vector3>(const Json& json, minEngine::Vector3& outValue)
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
    bool Serializer::Read<minEngine::Vector4>(const Json& json, minEngine::Vector4& outValue)
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


    Json Serializer::WriteByName(const std::string& typeName, Reflection::TypeCategory category, const void* value)
	{
        switch(category)
        {
            case Reflection::TypeCategory::Primitive:      return WriteByName_Primitive(typeName, value);
            case Reflection::TypeCategory::Object:         return WriteByName_Object(typeName, value);      
            case Reflection::TypeCategory::Enum:           return WriteByName_Enum(typeName, value);
            case Reflection::TypeCategory::Array:          return WriteByName_Array(typeName, value);       
        }

		ME_CORE_ERROR("[Serializer] No serialization function found for type '{}'", typeName);
		return Json();
	}

    Json Serializer::WriteByName_Primitive(const std::string& typeName, const void* value)
    {
        if (typeName == "int")
        {
            return Write<int>(*static_cast<const int*>(value));
        }
        else if (typeName == "float")
        {
            return Write<float>(*static_cast<const float*>(value));
        }
        else if (typeName == "double")
        {
            return Write<double>(*static_cast<const double*>(value));
        }
        else if (typeName == "bool")
        {
            return Write<bool>(*static_cast<const bool*>(value));
        }
        else if (typeName == "std::string")
        {
            return Write<std::string>(*static_cast<const std::string*>(value));
        }
        else if (typeName == "Vector2" || typeName == "minEngine::Vector2")
        {
            return Write<minEngine::Vector2>(*static_cast<const minEngine::Vector2*>(value));
        }
        else if (typeName == "Vector3" || typeName == "minEngine::Vector3")
        {
            return Write<minEngine::Vector3>(*static_cast<const minEngine::Vector3*>(value));
        }
        else if (typeName == "Vector4" || typeName == "minEngine::Vector4")
        {
            return Write<minEngine::Vector4>(*static_cast<const minEngine::Vector4*>(value));
        }

        ME_CORE_ERROR("[Serializer] No primitive serialization function found for type '{}'", typeName);
        return Json();
    }

    Json Serializer::WriteByName_Object(const std::string& typeName, const void* value)
    {
        // Check for registered types in the reflection system with write functions.
        const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo(typeName);
        if (typeInfo && typeInfo->writeToJson)
        {
            Json result = {{"$typeName", Json(typeName)},
                           {"$context", typeInfo->writeToJson(value)}};
            return result;
        }

        ME_CORE_ERROR("[Serializer] No object serialization function found for type '{}'", typeName);
        return Json();
    }

    Json Serializer::WriteByName_Enum(const std::string& typeName, const void* value)
    {
        // Check if the type is a enum and serialize it as its underlying integer type.
        const Reflection::EnumInfo* enumInfo = Reflection::GetEnumInfo(typeName);
        if (enumInfo)
        {
            const Reflection::EnumValueInfo* enumValueInfo = enumInfo->FindByValue(*static_cast<const int64_t*>(value));
            return Write<std::string>(enumValueInfo->name);
        }

        ME_CORE_ERROR("[Serializer] No enum serialization function found for type '{}'", typeName);
        return Json();
    }

    Json Serializer::WriteByName_Array(const std::string& typeName, const void* value)
    {
        // Check if it's an array type first, since array is also a "type" that has a TypeInfo
        const Reflection::ArrayTypeInfo* arrayTypeInfo = Reflection::GetArrayTypeInfo(typeName);
        if (arrayTypeInfo != nullptr)
        {
            Json arrayContext = Json::array();
            if (value == nullptr || arrayTypeInfo->getSize == nullptr || arrayTypeInfo->getConstElement == nullptr)
            {
                return arrayContext;
            }

            const size_t count = arrayTypeInfo->getSize(value);
            for (size_t index = 0; index < count; ++index)
            {
                const void* elementPtr = arrayTypeInfo->getConstElement(value, index);
                if (elementPtr == nullptr)
                {
                    arrayContext.push_back(Json());
                    continue;
                }

                arrayContext.push_back(WriteByName(arrayTypeInfo->elementTypeName, arrayTypeInfo->elementCategory, elementPtr));
            }

            return arrayContext;
        }

        ME_CORE_ERROR("[Serializer] No array serialization function found for type '{}'", typeName);
        return Json();
    }

    Json Serializer::WriteByName_Pointer(const std::string& typeName, const void* value)
    {
        if (value == nullptr)
        {
            return Json();
        }

        // TODO: Complete this
        return Json();
    }



    bool Serializer::ReadByName(const std::string& typeName, const Json& json, void* outValue)
    {
        if (outValue == nullptr)
        {
            return false;
        }

        // Check if it's an array type first, since array is also a "type" that has a TypeInfo
        const Reflection::ArrayTypeInfo* arrayTypeInfo = Reflection::GetArrayTypeInfo(typeName);
        if (arrayTypeInfo != nullptr)
        {
            if (!json.is_array() || arrayTypeInfo->resize == nullptr || arrayTypeInfo->getMutableElement == nullptr)
            {
                return false;
            }

            arrayTypeInfo->resize(outValue, json.size());
            for (size_t index = 0; index < json.size(); ++index)
            {
                void* elementPtr = arrayTypeInfo->getMutableElement(outValue, index);
                if (elementPtr == nullptr)
                {
                    return false;
                }

                if (!ReadByName(arrayTypeInfo->elementTypeName, json[index], elementPtr))
                {
                    return false;
                }
            }

            return true;
        }

        // Then check for registered types in the reflection system with read/write functions.
        const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo(typeName);
        if (typeInfo != nullptr)
        {
            const Json* context = &json;
            if (json.is_object() && json.contains("$context"))
            {
                context = &json["$context"];
            }

            if (!context->is_object())
            {
                return false;
            }

            for (const Reflection::FieldInfo& fieldInfo : typeInfo->fields)
            {
                if (!context->contains(fieldInfo.fieldName))
                {
                    continue;
                }

                void* fieldValuePtr = fieldInfo.mutableAccessor(outValue);
                if (fieldValuePtr == nullptr)
                {
                    return false;
                }

                if (!ReadByName(fieldInfo.fieldTypeName, (*context)[fieldInfo.fieldName], fieldValuePtr))
                {
                    return false;
                }
            }

            return true;
        }

        if(typeName == "int")
        {
            return Read<int>(json, *static_cast<int*>(outValue));
        }
        else if(typeName == "float")
        {
            return Read<float>(json, *static_cast<float*>(outValue));
        }
        else if(typeName == "double")
        {
            return Read<double>(json, *static_cast<double*>(outValue));
        }
        else if(typeName == "bool")
        {
            return Read<bool>(json, *static_cast<bool*>(outValue));
        }
        else if(typeName == "std::string")
        {
            return Read<std::string>(json, *static_cast<std::string*>(outValue));
        }
        else if(typeName == "Vector2" || typeName == "minEngine::Vector2")
        {
            return Read<minEngine::Vector2>(json, *static_cast<minEngine::Vector2*>(outValue));
        }
        else if(typeName == "Vector3" || typeName == "minEngine::Vector3")
        {
            return Read<minEngine::Vector3>(json, *static_cast<minEngine::Vector3*>(outValue));
        }
        else if(typeName == "Vector4" || typeName == "minEngine::Vector4")
        {
            return Read<minEngine::Vector4>(json, *static_cast<minEngine::Vector4*>(outValue));
        }

        // Lastly, check if the type is a enum and deserialize it as its underlying integer type.
        const Reflection::EnumInfo* enumInfo = Reflection::GetEnumInfo(typeName);
        if (enumInfo)
        {
            if (!json.is_string())
            {
                return false;
            }
            std::string enumValueName = json.get<std::string>();
            const Reflection::EnumValueInfo* enumValueInfo = enumInfo->FindByName(enumValueName);
            if (enumValueInfo == nullptr)
            {
                return false;
            }
            *static_cast<int64_t*>(outValue) = enumValueInfo->value;
            return true;
        }

        ME_CORE_ERROR("[Serializer] No deserialization function found for type '{}'", typeName);
        return false;
    }

}