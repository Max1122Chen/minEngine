#include "Serializer.h"


#include <fstream>

namespace minEngine
{

    template<typename T>
    bool Serializer::Read(const Json&, T& outValue)
    {
        static_assert(!std::is_same_v<T, T>, "Read is not implemented for this type.");
        return false;
    }

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


    Json Serializer::WriteByName(const std::string& typeName, const void* value)
	{
        // Check if the type has a registered writeToJson function in the reflection system first.
		const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo(typeName);
		if (typeInfo && typeInfo->writeToJson)
		{
			return typeInfo->writeToJson(value);
		}
		
        // Then check for built-in types with explicit specializations.
		if(typeName == "int")
		{
			return Write<int>(*static_cast<const int*>(value));
		}
		else if(typeName == "float")
		{
			return Write<float>(*static_cast<const float*>(value));
		}
		else if(typeName == "double")
		{
			return Write<double>(*static_cast<const double*>(value));
		}
		else if(typeName == "bool")
		{
			return Write<bool>(*static_cast<const bool*>(value));
		}
		else if(typeName == "std::string")
		{
			return Write<std::string>(*static_cast<const std::string*>(value));
		}
		else if(typeName == "minEngine::Vector2")
		{
			return Write<minEngine::Vector2>(*static_cast<const minEngine::Vector2*>(value));
		}
		else if(typeName == "minEngine::Vector3")
		{
			return Write<minEngine::Vector3>(*static_cast<const minEngine::Vector3*>(value));
		}
		else if(typeName == "minEngine::Vector4")
		{
			return Write<minEngine::Vector4>(*static_cast<const minEngine::Vector4*>(value));
		}

        // Lastly, check if the type is a enum and serialize it as its underlying integer type.
        const Reflection::EnumInfo* enumInfo = Reflection::GetEnumInfo(typeName);
        if (enumInfo)
        {
            const Reflection::EnumValueInfo* enumValueInfo = enumInfo->FindByValue(*static_cast<const int64_t*>(value));
            return Write<std::string>(enumValueInfo->name);
        }

		ME_CORE_ERROR("[Serializer] No serialization function found for type '{}'", typeName);
		return Json();
	}

}