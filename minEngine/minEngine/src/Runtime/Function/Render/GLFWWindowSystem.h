#pragma once
#include "Core.h"
#include "WindowSystem.h"
#include "PointerConstraintState.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"

struct GLFWwindow;

namespace minEngine
{
    class GLFWWindowSystem : public WindowSystem, private ICursorConstraintSink
    {
    public:
        int m_Width;
        int m_Height;
        GLFWwindow* m_Window = nullptr;

        GLFWWindowSystem(int width = 800, int height = 600)
            : m_Width(width), m_Height(height), m_Window(nullptr), m_PointerConstraint(*this)
        {
        }

        ~GLFWWindowSystem();

        virtual void Initialize() override;
        virtual void Shutdown() override;
        virtual bool ShouldClose() const override;
        virtual void Close() override;
        virtual void SetTitle(const char* title) override;

        virtual void SetClearColor(Vector3 color) override;
        virtual void Clear() override;

        virtual void SwapBuffers() override;
        virtual void MakeGraphicsContextCurrent() override;
        virtual void ClearGraphicsContextCurrent() override;
        virtual void PollEvents() const override;
        virtual void SetCursorVisible(bool visible) override;
        virtual void SetCursorClipRect(const WindowCursorClipRect& rect) override;
        virtual Vector2 ConsumeRelativeMouseDelta() override;
        virtual WindowCursorConstraintState GetCursorConstraintState() const override;

        virtual void* GetWindowHandle() const override { return m_Window; }

        virtual uint32_t GetWidth() override { return m_Width; }
        virtual uint32_t GetHeight() override { return m_Height; }

        bool HasOpenGLContext() const { return m_HasOpenGLContext; }

        /** Called from Win32 WndProc while Raw Input is registered. */
        void AccumulateRelativeMouseDelta(float deltaX, float deltaY);

        /**
         * Platform-internal focus entry, forwarded from the GLFW focus callback and from WM_ACTIVATE.
         * Focus loss revokes every OS-level constraint immediately; focus gain replays the request in force.
         * Not a game-facing API.
         */
        void OnWindowFocusChanged(bool focused);

    protected:
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem == nullptr)
            {
                return;
            }

            const InputKey* inputKey = nullptr;
            if (!TryConvertGLFWKeyToInputKey(key, inputKey) || inputKey == nullptr)
            {
                return;
            }

            const InputKeyAction keyAction = ConvertGLFWKeyActionToInputKeyAction(action);
            windowSystem->OnKey(*inputKey, scancode, keyAction, mods);
        }

        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem == nullptr)
            {
                return;
            }

            const InputKey* inputKey = nullptr;
            if (!TryConvertGLFWKeyToInputKey(button, inputKey) || inputKey == nullptr)
            {
                return;
            }

            const InputKeyAction keyAction = ConvertGLFWKeyActionToInputKeyAction(action);
            windowSystem->OnKey(*inputKey, 0, keyAction, mods);
        }

        static void CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnCursorPos(xPos, yPos);
            }
        }

        static void MouseScrollCallback(GLFWwindow* window, double xOffset, double yOffset)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnMouseScroll(xOffset, yOffset);
            }
        }

        static void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem)
            {
                windowSystem->OnWindowSize(width, height);
            }
        }

        void OnKey(InputKey key, int scancode, InputKeyAction action, int mods);
        void OnMouseButton(InputKey key, InputKeyAction action, int mods);
        void OnCursorPos(double xPos, double yPos);
        void OnMouseScroll(double xOffset, double yOffset);
        void OnWindowSize(int width, int height);

    private:
        GLFWWindowSystem() = delete;

        bool m_IsGlfwInitialized = false;
        bool m_IsInitialized = false;
        bool m_HasOpenGLContext = false;
        bool m_bRawMouseRegistered = false;
        mutable Vector2 m_PendingRelativeMouseDelta = Vector2(0.0f, 0.0f);
        // Mutable: PollEvents() (const) feeds the watchdog with the platform focus truth every frame.
        mutable PointerConstraintState m_PointerConstraint;

        void SetupWindowEventCallbacks();
        void SetRelativeMouseCaptureEnabled(bool enabled);
        void SyncMouseIntentPreserveWithClip(bool clipActive);

        // ICursorConstraintSink
        WindowCursorClipRect GetClientScreenRect() const override;
        void ApplyOsClip(const WindowCursorClipRect& rect) override;
        void ApplyCursorMode(EWindowCursorMode mode) override;
        void ApplyRelativeCapture(bool enabled) override;
        /** Returns false for unmapped codes (do not write AnyKey). */
        static bool TryConvertGLFWKeyToInputKey(int glfwKey, const InputKey*& outKey);
        static InputKeyAction ConvertGLFWKeyActionToInputKeyAction(int action);
    };
}