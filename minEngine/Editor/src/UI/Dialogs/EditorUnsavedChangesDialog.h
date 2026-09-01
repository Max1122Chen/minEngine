#pragma once

#include "Core.h"

namespace minEngine
{
    enum class UnsavedChangesChoice
    {
        None,
        Save,
        Discard,
        Cancel
    };

    class EditorUnsavedChangesDialog
    {
    public:
        void Open(const char* message);
        UnsavedChangesChoice Draw();
        bool IsOpen() const { return m_Open; }
        void Close();

    private:
        bool m_Open = false;
        char m_Message[512] = {};
    };
}
