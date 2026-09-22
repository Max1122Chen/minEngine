#pragma once

#include "Core.h"

#include "Runtime/Function/Render/Sprite/Sprite.h"

#include <memory>
#include <string>

namespace minEngine
{
    struct SpriteEditorSession
    {
        std::shared_ptr<Sprite> SpriteAsset;
        std::string AssetPath;
        bool Dirty = false;

        bool HasOpenSprite() const
        {
            return SpriteAsset != nullptr && !AssetPath.empty();
        }

        void Clear()
        {
            SpriteAsset.reset();
            AssetPath.clear();
            Dirty = false;
        }
    };
}
