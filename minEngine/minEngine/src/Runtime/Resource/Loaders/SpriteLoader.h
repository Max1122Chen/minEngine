#pragma once

#include "Core.h"
#include "Runtime/Resource/AssetMeta.h"

#include <memory>
#include <string>

namespace minEngine
{
    class Sprite;
    class SpriteAtlas;

    class SpriteLoader
    {
    public:
        static std::shared_ptr<Sprite> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const Sprite& asset, std::string* outError = nullptr);
    };

    class SpriteAtlasLoader
    {
    public:
        static std::shared_ptr<SpriteAtlas> Load(const AssetMeta& meta, std::string* outError = nullptr);
        static bool Save(const AssetMeta& meta, const SpriteAtlas& asset, std::string* outError = nullptr);
    };
}
