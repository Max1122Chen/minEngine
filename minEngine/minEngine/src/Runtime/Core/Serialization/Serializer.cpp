#include "Serializer.h"

#include <fstream>

namespace minEngine
{
    template<typename T>
    Json Serializer::Write(const T& value)
    {
        static_assert(!std::is_same_v<T, T>, "Write is not implemented for this type.");
        return Json();
    }

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

}