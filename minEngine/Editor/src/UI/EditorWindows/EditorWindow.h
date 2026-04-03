#pragma once

#include "Core.h"

namespace minEngine
{
    class Editor;
}

namespace minEngine
{
    class EditorWindow
    {
    public:
        explicit EditorWindow(Editor& editor)
            : m_Editor(editor)
        {
        }

        virtual ~EditorWindow() = default;

        virtual const std::string& GetId() const = 0;
        virtual const std::string& GetTitle() const = 0;

        bool IsOpen() const
        {
            return m_IsOpen;
        }

        void SetOpen(bool isOpen)
        {
            m_IsOpen = isOpen;
        }

        virtual void OnAttach()
        {
        }

        virtual void OnDetach()
        {
        }

        virtual void OnTick()
        {
        }

        virtual void OnDraw() = 0;

    private:
    protected:
        Editor& m_Editor;

    private:
        bool m_IsOpen = true;
    };
}
