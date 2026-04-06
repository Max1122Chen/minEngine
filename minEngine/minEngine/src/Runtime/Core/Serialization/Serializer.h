#pragma once

#include "Runtime/Core/Core.h"

#include "../../../../Third-Party/json/json.hpp"

#include <filesystem>

namespace minEngine
{
	class Serializer
	{
	public:
		using Json = nlohmann::json;

		template<typename TValue>
		static Json Write(const TValue& value);

		template<typename TValue>
		static bool Read(const Json& json, TValue& outValue);

		static Json ToJsonByTypeInfo(const void* object, const Reflection::TypeInfo& typeInfo);
		static bool FromJsonByTypeInfo(const Json& json, void* object, const Reflection::TypeInfo& typeInfo);

		template<typename TObject>
		static Json ToJson(const TObject& object)
		{
			const Reflection::TypeInfo* typeInfo = Reflection::ReflectionSystem::Get().GetTypeInfo<TObject>();
			if (typeInfo == nullptr)
			{
				ME_CORE_ERROR("[Serializer] Type is not registered in reflection system.");
				return Json::object();
			}

			return ToJsonByTypeInfo(&object, *typeInfo);
		}

		template<typename TObject>
		static bool FromJson(const Json& json, TObject& object)
		{
			const Reflection::TypeInfo* typeInfo = Reflection::ReflectionSystem::Get().GetTypeInfo<TObject>();
			if (typeInfo == nullptr)
			{
				ME_CORE_ERROR("[Serializer] Type is not registered in reflection system.");
				return false;
			}

			return FromJsonByTypeInfo(json, &object, *typeInfo);
		}

		static bool SaveJsonToFile(const Json& json, const std::filesystem::path& filePath);

		template<typename TObject>
		static bool SaveToFile(const TObject& object, const std::filesystem::path& filePath)
		{
			return SaveJsonToFile(ToJson(object), filePath);
		}

		static bool LoadJsonFromFile(const std::filesystem::path& filePath, Json& outJson);

		template<typename TObject>
		static bool LoadFromFile(const std::filesystem::path& filePath, TObject& outObject)
		{
			Json json;
			if (!LoadJsonFromFile(filePath, json))
			{
				return false;
			}

			return FromJson(json, outObject);
		}

	private:
		static bool WriteFieldValue(const Reflection::FieldInfo& field, const void* fieldPtr, Json& outValue);
		static bool ReadFieldValue(const Reflection::FieldInfo& field, const Json& value, void* fieldPtr);
	};

	// Explicit specialization declarations.
	template<>
	Serializer::Json Serializer::Write<int>(const int& value);
	template<>
	Serializer::Json Serializer::Write<float>(const float& value);
	template<>
	Serializer::Json Serializer::Write<double>(const double& value);
	template<>
	Serializer::Json Serializer::Write<bool>(const bool& value);
	template<>
	Serializer::Json Serializer::Write<std::string>(const std::string& value);
	template<>
	Serializer::Json Serializer::Write<Vector2>(const Vector2& value);
	template<>
	Serializer::Json Serializer::Write<Vector3>(const Vector3& value);
	template<>
	Serializer::Json Serializer::Write<Vector4>(const Vector4& value);

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
	bool Serializer::Read<Vector2>(const Json& json, Vector2& outValue);
	template<>
	bool Serializer::Read<Vector3>(const Json& json, Vector3& outValue);
	template<>
	bool Serializer::Read<Vector4>(const Json& json, Vector4& outValue);
}

