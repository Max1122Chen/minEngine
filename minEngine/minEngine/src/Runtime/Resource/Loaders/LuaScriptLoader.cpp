#include "Runtime/Resource/Loaders/LuaScriptLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/LuaScript.h"

#include <fstream>
#include <sstream>

namespace minEngine
{
    bool LuaScriptLoader::ReadScriptFileText(
        const std::filesystem::path& absolutePath,
        std::string& outText,
        std::string* outError)
    {
        std::ifstream inputFile(absolutePath, std::ios::in | std::ios::binary);
        if (!inputFile.is_open())
        {
            const std::string message = "Failed to open Lua script: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        std::ostringstream buffer;
        buffer << inputFile.rdbuf();
        if (!inputFile.good() && !inputFile.eof())
        {
            const std::string message = "Failed to read Lua script: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        outText = buffer.str();
        if (outText.empty())
        {
            const std::string message = "Lua script is empty: " + absolutePath.string();
            if (outError != nullptr)
            {
                *outError = message;
            }
            ME_CORE_ERROR("{}", message);
            return false;
        }

        return true;
    }

    std::shared_ptr<LuaScript> LuaScriptLoader::LoadFromAssetMeta(const AssetMeta& meta)
    {
        const std::filesystem::path absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath);
        std::string source;
        std::string error;
        if (!ReadScriptFileText(absoluteAssetPath, source, &error))
        {
            ME_CORE_ERROR("LuaScriptLoader: failed to load '{}' ({}).", meta.AssetPath, error);
            return nullptr;
        }

        std::shared_ptr<LuaScript> script = NewObject<LuaScript>(meta.AssetName, nullptr, meta.Guid);
        script->m_Source = std::move(source);
        return script;
    }

    template<>
    std::shared_ptr<LuaScript> AssetManager::LoadAsset_Impl<LuaScript>(const AssetMeta& meta)
    {
        return LuaScriptLoader::LoadFromAssetMeta(meta);
    }
}
