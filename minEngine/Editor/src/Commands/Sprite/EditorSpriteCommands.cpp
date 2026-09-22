#include "Commands/Sprite/EditorSpriteCommands.h"

namespace minEngine
{
    EditorSetSpriteSourceRectCommand::EditorSetSpriteSourceRectCommand(Sprite& sprite, Vector4 afterRectPx)
        : m_Sprite(sprite)
        , m_AfterRectPx(afterRectPx)
    {
    }

    void EditorSetSpriteSourceRectCommand::Execute()
    {
        if (!m_Captured)
        {
            m_BeforeRectPx = m_Sprite.GetSourceRectPx();
            m_Captured = true;
        }

        m_Sprite.SetSourceRectPx(m_AfterRectPx);
    }

    void EditorSetSpriteSourceRectCommand::Undo()
    {
        m_Sprite.SetSourceRectPx(m_BeforeRectPx);
    }

    const char* EditorSetSpriteSourceRectCommand::GetDescription() const
    {
        m_Description = "Set Sprite Source Rect";
        return m_Description.c_str();
    }

    EditorAddAtlasMembersCommand::EditorAddAtlasMembersCommand(
        SpriteAtlas& atlas,
        std::vector<std::shared_ptr<Sprite>> sprites)
        : m_Atlas(atlas)
        , m_Sprites(std::move(sprites))
    {
    }

    void EditorAddAtlasMembersCommand::Execute()
    {
        m_Added.clear();
        for (const std::shared_ptr<Sprite>& sprite : m_Sprites)
        {
            if (!sprite)
            {
                continue;
            }

            std::string error;
            if (m_Atlas.AddMember(sprite, &error))
            {
                m_Added.push_back(sprite);
            }
        }
    }

    void EditorAddAtlasMembersCommand::Undo()
    {
        for (const std::shared_ptr<Sprite>& sprite : m_Added)
        {
            m_Atlas.RemoveMember(sprite);
        }
        m_Added.clear();
    }

    const char* EditorAddAtlasMembersCommand::GetDescription() const
    {
        m_Description = "Add Sprite Atlas Members";
        return m_Description.c_str();
    }

    EditorRemoveAtlasMemberCommand::EditorRemoveAtlasMemberCommand(
        SpriteAtlas& atlas,
        std::shared_ptr<Sprite> sprite)
        : m_Atlas(atlas)
        , m_Sprite(std::move(sprite))
    {
    }

    void EditorRemoveAtlasMemberCommand::Execute()
    {
        m_Removed = m_Sprite && m_Atlas.RemoveMember(m_Sprite);
    }

    void EditorRemoveAtlasMemberCommand::Undo()
    {
        if (!m_Removed || !m_Sprite)
        {
            return;
        }

        m_Atlas.AddMember(m_Sprite);
        m_Removed = false;
    }

    const char* EditorRemoveAtlasMemberCommand::GetDescription() const
    {
        m_Description = "Remove Sprite Atlas Member";
        return m_Description.c_str();
    }
}
