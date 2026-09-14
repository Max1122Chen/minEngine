#include "Shell/Document/EditorDocumentTypeRegistry.h"

namespace minEngine
{
    void EditorDocumentTypeRegistry::Register(EditorDocumentTypeInfo info)
    {
        if (info.TypeId.empty())
        {
            return;
        }

        for (EditorDocumentTypeInfo& existing : m_Types)
        {
            if (existing.TypeId == info.TypeId)
            {
                existing = std::move(info);
                return;
            }
        }
        m_Types.push_back(std::move(info));
    }

    const EditorDocumentTypeInfo* EditorDocumentTypeRegistry::Find(std::string_view typeId) const
    {
        for (const EditorDocumentTypeInfo& info : m_Types)
        {
            if (info.TypeId == typeId)
            {
                return &info;
            }
        }
        return nullptr;
    }

    const EditorDocumentTypeInfo* EditorDocumentTypeRegistry::FindForAsset(const AssetMeta& meta) const
    {
        for (const EditorDocumentTypeInfo& info : m_Types)
        {
            if (info.CanOpenAsset && info.CanOpenAsset(meta))
            {
                return &info;
            }
        }
        return nullptr;
    }
}
