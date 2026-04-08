#pragma once

#include "Runtime/Core/Core.h"

#include "Json.h"

#include <filesystem>

namespace minEngine
{
	class Serializer
	{
	public:

		static Json WriteByName(const std::string& typeName, const void* value);

		template<typename T>
		static Json Write(const T& value);

		template<typename T>
		static bool Read(const Json& json, T& outValue);

	};


	template<typename T>
    Json Serializer::Write(const T& value)
    {
        Json result = Json::object();
        const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo<T>();
        if (typeInfo)
        {
            for (const auto& fieldInfo : typeInfo->fields)
            {
                const Reflection::TypeInfo* fieldTypeInfo = Reflection::GetTypeInfo(fieldInfo.fieldTypeName);
                if(fieldTypeInfo && fieldTypeInfo->writeToJson)
                {
                    const void* fieldValuePtr = fieldInfo.constAccessor(&value);
                    if (fieldValuePtr)
                    {
                        result[fieldInfo.fieldName] = WriteByName(fieldInfo.fieldTypeName, fieldValuePtr);
                    }
                    else
                    {
                        ME_CORE_ERROR("[Serializer] Failed to access field '{}' of type '{}'", fieldInfo.fieldName, typeInfo->typeName);
                    }
                }
            }
        }
        return result;
    }

	// Explicit specialization declarations.
	template<>
	Json Serializer::Write<int>(const int& value);
	template<>
	Json Serializer::Write<float>(const float& value);
	template<>
	Json Serializer::Write<double>(const double& value);
	template<>
	Json Serializer::Write<bool>(const bool& value);
	template<>
	Json Serializer::Write<std::string>(const std::string& value);
	template<>
	Json Serializer::Write<minEngine::Vector2>(const minEngine::Vector2& value);
	template<>
	Json Serializer::Write<minEngine::Vector3>(const minEngine::Vector3& value);
	template<>
	Json Serializer::Write<minEngine::Vector4>(const minEngine::Vector4& value);

	template<>
	bool Serializer::Read<int>(const Json& json, int& outValue);
	template<>
	bool Serializer::Read<float>(const Json& json, float& outValue);
	template<>
	bool Serializer::Read<double>(const Json& json, double& outValue);
	template<>
	bool Serializer::Read<bool>(const Json& json, bool& outValue);
	template<>
	bool Serializer::Read<std::string>(const Json& json, std::string& outValue);
	template<>
	bool Serializer::Read<minEngine::Vector2>(const Json& json, minEngine::Vector2& outValue);
	template<>
	bool Serializer::Read<minEngine::Vector3>(const Json& json, minEngine::Vector3& outValue);
	template<>
	bool Serializer::Read<minEngine::Vector4>(const Json& json, minEngine::Vector4& outValue);
}

