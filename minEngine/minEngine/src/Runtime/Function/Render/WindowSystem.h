#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
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

        // Window event callback function types
        typedef std::function<void()>                       OnResetFunc;
        typedef std::function<void(int, int, int, int)>     OnKeyFunc;
        typedef std::function<void(int, int)>               OnMouseButtonFunc;
        typedef std::function<void(double, double)>         OnCursorPosFunc;
        typedef std::function<void(int, int)>               OnWindowSizeFunc;

        void RegisterOnResetCallback(const OnResetFunc& callback) { m_OnResetCallbacks.push_back(callback); }
        void RegisterOnKeyCallback(const OnKeyFunc& callback) { m_OnKeyCallbacks.push_back(callback); }
        void RegisterOnMouseButtonCallback(const OnMouseButtonFunc& callback) { m_OnMouseButtonCallbacks.push_back(callback); }
        void RegisterOnCursorPosCallback(const OnCursorPosFunc& callback) { m_OnCursorPosCallbacks.push_back(callback); }
        void RegisterOnWindowSizeCallback(const OnWindowSizeFunc& callback) { m_OnWindowSizeCallbacks.push_back(callback); }

    protected:
        std::vector<OnResetFunc>            m_OnResetCallbacks;
        std::vector<OnKeyFunc>              m_OnKeyCallbacks;
        std::vector<OnMouseButtonFunc>      m_OnMouseButtonCallbacks;
        std::vector<OnCursorPosFunc>        m_OnCursorPosCallbacks;
        std::vector<OnWindowSizeFunc>       m_OnWindowSizeCallbacks;

    };
}

