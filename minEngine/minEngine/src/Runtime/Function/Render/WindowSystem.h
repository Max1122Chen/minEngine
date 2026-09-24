#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include "Input/InputKeys.h"
#include "Input/InputKeyTypes.h"

namespace minEngine
{
    class Engine;

    /** Inclusive-exclusive screen pixel rect for OS cursor clip (Win32 ClipCursor). */
    struct WindowCursorClipRect
    {
        bool bEnabled = false;
        int Left = 0;
        int Top = 0;
        int Right = 0;
        int Bottom = 0;
    };

    class WindowSystem
    {
    public:
        WindowSystem() = default;
        virtual ~WindowSystem() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        virtual bool ShouldClose() const = 0;
        virtual void Close() = 0;
        virtual void SetTitle(const char* title) = 0;

        virtual void SetClearColor(Vector3 color) = 0;
        virtual void Clear() = 0;

        virtual void SwapBuffers() = 0;
        virtual void PollEvents() const = 0;
        virtual void SetCursorVisible(bool visible) = 0;
        /** Confine OS cursor to a screen-space rect; bEnabled=false clears. Win32 ClipCursor; other platforms may no-op or degrade. */
        virtual void SetCursorClipRect(const WindowCursorClipRect& rect) = 0;
        /**
         * Relative mouse motion accumulated since last consume (e.g. Win32 Raw Input while clipped).
         * Zero when unsupported or idle. Used so look intent continues at ClipCursor edges.
         */
        virtual Vector2 ConsumeRelativeMouseDelta() { return Vector2(0.0f, 0.0f); }

        virtual void* GetWindowHandle() const = 0;

        virtual uint32_t GetWidth() = 0;
        virtual uint32_t GetHeight() = 0;

        static bool HasInstance() { return s_Instance != nullptr; }
        static WindowSystem& Get();

        // Window event callback function types
        typedef std::function<void()>                                              OnResetFunc;
        typedef std::function<void(InputKey, int, InputKeyAction, int)>            OnKeyFunc;
        typedef std::function<void(InputKey, InputKeyAction, int)>                 OnMouseButtonFunc;
        typedef std::function<void(double, double)>                                OnCursorPosFunc;
        typedef std::function<void(double, double)>                                OnMouseScrollFunc;
        typedef std::function<void(int, int)>                                      OnWindowSizeFunc;

        void RegisterOnResetCallback(const OnResetFunc& callback) { m_OnResetCallbacks.push_back(callback); }
        void RegisterOnKeyCallback(const OnKeyFunc& callback) { m_OnKeyCallbacks.push_back(callback); }
        void RegisterOnMouseButtonCallback(const OnMouseButtonFunc& callback) { m_OnMouseButtonCallbacks.push_back(callback); }
        void RegisterOnCursorPosCallback(const OnCursorPosFunc& callback) { m_OnCursorPosCallbacks.push_back(callback); }
        void RegisterOnMouseScrollCallback(const OnMouseScrollFunc& callback) { m_OnMouseScrollCallbacks.push_back(callback); }
        void RegisterOnWindowSizeCallback(const OnWindowSizeFunc& callback) { m_OnWindowSizeCallbacks.push_back(callback); }

    private:
        friend class Engine;

        static void SetInstance(WindowSystem* instance);
        static WindowSystem* s_Instance;

    protected:
        std::vector<OnResetFunc>            m_OnResetCallbacks;
        std::vector<OnKeyFunc>              m_OnKeyCallbacks;
        std::vector<OnMouseButtonFunc>      m_OnMouseButtonCallbacks;
        std::vector<OnCursorPosFunc>        m_OnCursorPosCallbacks;
        std::vector<OnMouseScrollFunc>      m_OnMouseScrollCallbacks;
        std::vector<OnWindowSizeFunc>       m_OnWindowSizeCallbacks;

    };
}

