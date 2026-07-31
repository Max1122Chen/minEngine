#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <filesystem>
#include <memory>
#include <string>

namespace minEngine
{
    class LuaScript;

    class LuaScriptLoader
    {
    public:
        static std::shared_ptr<LuaScript> LoadFromAssetMeta(const AssetMeta& meta);

    private:
        static bool ReadScriptFileText(
            const std::filesystem::path& absolutePath,
            std::string& outText,
            std::string* outError = nullptr);
    };
}
