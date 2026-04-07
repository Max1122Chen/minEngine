#pragma once

#include "Runtime/Core/Core.h"
#include "Runtime/Core/Math/Math.h"

#include "Json.h"

#include <filesystem>

namespace minEngine
{
	class Serializer
	{
	public:

		template<typename T>
		static Json Write(const T& value);

		template<typename T>
		static bool Read(const Json& json, T& outValue);

		template<typename TObject>
		static Json ToJson(const TObject& object)
		{
			return Write(object);
		}

		template<typename TObject>
		static bool FromJson(const Json& json, TObject& object)
		{
			return Read(json, object);
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
	};

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

