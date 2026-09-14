#pragma once

#include "Shell/Document/EditorDocumentId.h"
#include "Shell/EditorCommandStack.h"

#include <string>

namespace minEngine
{
    class EditorDocumentSession
    {
    public:
        EditorDocumentSession(EditorDocumentId id, std::string typeId, std::string assetKey, std::string title);

        EditorDocumentId GetId() const { return m_Id; }
        const std::string& GetTypeId() const { return m_TypeId; }
        const std::string& GetAssetKey() const { return m_AssetKey; }
        const std::string& GetTitle() const { return m_Title; }
        void SetTitle(std::string title) { m_Title = std::move(title); }

        bool IsDirty() const { return m_Dirty; }
        void SetDirty(bool dirty) { m_Dirty = dirty; }

        bool IsPinned() const { return m_Pinned; }
        void SetPinned(bool pinned) { m_Pinned = pinned; }

        EditorCommandStack& GetCommandStack() { return m_CommandStack; }
        const EditorCommandStack& GetCommandStack() const { return m_CommandStack; }

    private:
        EditorDocumentId m_Id;
        std::string m_TypeId;
        std::string m_AssetKey;
        std::string m_Title;
        bool m_Dirty = false;
        bool m_Pinned = false;
        EditorCommandStack m_CommandStack;
    };
}
