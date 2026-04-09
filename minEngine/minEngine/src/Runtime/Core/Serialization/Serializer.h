#pragma once

#include "Runtime/Core/Core.h"

#include "Json.h"

#include <filesystem>

namespace minEngine
{
	class Serializer
	{
	public:

		static Json WriteByName(const std::string& typeName, Reflection::TypeCategory category, const void* value);
		static bool ReadByName(const std::string& typeName, const Json& json, void* outValue);

		template<typename T>
		static Json Write(const T& value);

		template<typename T>
		static Json WritePointer(const T*& value);

		template<typename T>
		static bool Read(const Json& json, T& outValue);

	private:
		Serializer() = delete;
		Serializer(const Serializer&) = delete;
		Serializer& operator=(const Serializer&) = delete;

		static Json WriteByName_Primitive(const std::string& typeName, const void* value);
		static Json WriteByName_Object(const std::string& typeName, const void* value);
		static Json WriteByName_Enum(const std::string& typeName, const void* value);
		static Json WriteByName_Array(const std::string& typeName, const void* value);
		static Json WriteByName_Pointer(const std::string& typeName, const void* value);

		// TODO: read by name for different categories as well.

	};

	// Template Function for class/structs that are reflected
	template<typename T>
    Json Serializer::Write(const T& value)
    {
		// Write Object
        Json result;
        const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo<T>();
        if (typeInfo)
        {
            for (const auto& fieldInfo : typeInfo->fields)
            {
				const void* fieldValuePtr = fieldInfo.constAccessor(&value);
				if (fieldValuePtr)
                {
					result[fieldInfo.fieldName] = WriteByName(fieldInfo.fieldTypeName, fieldInfo.category, fieldValuePtr);
				}
				else
				{
					ME_CORE_ERROR("[Serializer] Failed to access field '{}' of type '{}'", fieldInfo.fieldName, typeInfo->typeName);
                }
            }
        }
		else
		{
			ME_CORE_ERROR("[Serializer] Write is not implemented for type '{}'", typeid(T).name());
		}
        return result;
    }

	template<typename T>
	Json Serializer::WritePointer(const T*& value)
	{
		if(value == nullptr)
		{
			return Json();
		}
		return Write<T>(*value);
	}

	template<typename T>
	bool Serializer::Read(const Json& json, T& outValue)
	{
		const Reflection::TypeInfo* typeInfo = Reflection::GetTypeInfo<T>();
		if (typeInfo == nullptr)
		{
			ME_CORE_ERROR("[Serializer] Read is not implemented for type '{}'", typeid(T).name());
			return false;
		}

		const Json* context = &json;
		if (json.is_object() && json.contains("$context"))
		{
			context = &json["$context"];
		}

		if (!context->is_object())
		{
			return false;
		}

		for (const auto& fieldInfo : typeInfo->fields)
		{
			if (!context->contains(fieldInfo.fieldName))
			{
				continue;
			}

			void* fieldValuePtr = fieldInfo.mutableAccessor(&outValue);
			if (fieldValuePtr == nullptr)
			{
				ME_CORE_ERROR("[Serializer] Failed to access mutable field '{}' of type '{}'", fieldInfo.fieldName, typeInfo->typeName);
				return false;
			}

			if (!ReadByName(fieldInfo.fieldTypeName, (*context)[fieldInfo.fieldName], fieldValuePtr))
			{
				ME_CORE_ERROR("[Serializer] Failed to deserialize field '{}' of type '{}'", fieldInfo.fieldName, typeInfo->typeName);
				return false;
			}
		}

		return true;
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

