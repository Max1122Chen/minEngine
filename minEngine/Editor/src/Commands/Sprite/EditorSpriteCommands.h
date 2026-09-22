#pragma once

#include "Shell/EditorCommandStack.h"

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class EditorSetSpriteSourceRectCommand final : public EditorCommand
    {
    public:
        EditorSetSpriteSourceRectCommand(Sprite& sprite, Vector4 afterRectPx);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        Sprite& m_Sprite;
        Vector4 m_BeforeRectPx{ 0.0f, 0.0f, 0.0f, 0.0f };
        Vector4 m_AfterRectPx{ 0.0f, 0.0f, 0.0f, 0.0f };
        bool m_Captured = false;
        mutable std::string m_Description;
    };

    class EditorAddAtlasMembersCommand final : public EditorCommand
    {
    public:
        EditorAddAtlasMembersCommand(
            SpriteAtlas& atlas,
            std::vector<std::shared_ptr<Sprite>> sprites);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SpriteAtlas& m_Atlas;
        std::vector<std::shared_ptr<Sprite>> m_Sprites;
        std::vector<std::shared_ptr<Sprite>> m_Added;
        mutable std::string m_Description;
    };

    class EditorRemoveAtlasMemberCommand final : public EditorCommand
    {
    public:
        EditorRemoveAtlasMemberCommand(SpriteAtlas& atlas, std::shared_ptr<Sprite> sprite);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        SpriteAtlas& m_Atlas;
        std::shared_ptr<Sprite> m_Sprite;
        bool m_Removed = false;
        mutable std::string m_Description;
    };
}
