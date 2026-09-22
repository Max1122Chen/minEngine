#pragma once

#include "Core.h"

#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"

#include <memory>
#include <string>

namespace minEngine
{
    struct SpriteAtlasEditorSession
    {
        std::shared_ptr<SpriteAtlas> AtlasAsset;
        std::string AssetPath;
        bool Dirty = false;
        std::string LastRebuildMessage;

        bool HasOpenAtlas() const
        {
            return AtlasAsset != nullptr && !AssetPath.empty();
        }

        void Clear()
        {
            AtlasAsset.reset();
            AssetPath.clear();
            Dirty = false;
            LastRebuildMessage.clear();
        }
    };
}
