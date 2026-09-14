#pragma once

#include "Shell/Document/EditorDocumentId.h"
#include "Runtime/Resource/AssetMeta.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class IEditorContext;
    class EditorDocumentSession;

    struct EditorDocumentTypeInfo
    {
        std::string TypeId;
        std::string DisplayName;
        std::string ModuleId;
        bool AllowMultipleSessions = true;
        bool AllowCloseLastOfType = true;

        std::function<bool(const AssetMeta&)> CanOpenAsset;
        std::function<bool(IEditorContext&, const AssetMeta&, EditorDocumentSession&)> BindSession;
        std::function<bool(IEditorContext&, EditorDocumentSession&)> ActivateSession;
        std::function<void(IEditorContext&, EditorDocumentSession&)> CloseSession;
        std::function<bool(IEditorContext&, const EditorDocumentSession&)> QueryDirty;
        std::function<bool(IEditorContext&, EditorDocumentSession&)> SaveSession;
        std::function<std::string(const AssetMeta&)> MakeAssetKey;
        std::function<std::string(const AssetMeta&)> MakeTitle;
    };

    class EditorDocumentTypeRegistry
    {
    public:
        void Register(EditorDocumentTypeInfo info);
        const EditorDocumentTypeInfo* Find(std::string_view typeId) const;
        const EditorDocumentTypeInfo* FindForAsset(const AssetMeta& meta) const;
        const std::vector<EditorDocumentTypeInfo>& GetTypes() const { return m_Types; }

    private:
        std::vector<EditorDocumentTypeInfo> m_Types;
    };
}
