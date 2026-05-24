#pragma once

#include "Core.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorContext.h"

#include <string_view>

namespace minEngine
{
    class EditorWindow
    {
    public:
        explicit EditorWindow(IEditorContext& context)
            : m_Context(context)
        {
        }

        virtual ~EditorWindow() = default;

        virtual const std::string& GetId() const = 0;
        virtual const std::string& GetTitle() const = 0;

        /** Empty = shared (always visible). Otherwise must match active SubModule id. */
        virtual std::string_view GetOwnerModuleId() const { return std::string_view(); }

        bool IsOpen() const { return m_IsOpen; }
        void SetOpen(bool isOpen) { m_IsOpen = isOpen; }

        bool IsVisibleForActiveModule() const;

        virtual void OnAttach() {}
        virtual void OnDetach() {}

        virtual void OnTick() {}
        virtual void OnDraw() = 0;

    protected:
        IEditorContext& m_Context;

    private:
        bool m_IsOpen = true;
    };
}
