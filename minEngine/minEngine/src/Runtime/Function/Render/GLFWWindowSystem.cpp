#include "GLFWWindowSystem.h"
#include "WindowSystem.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Input/InputSystem.h"

#include "Core.h"

#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

// TODO: Include error handling and logging as needed



namespace minEngine
{
#if defined(_WIN32)
    namespace
    {
        GLFWWindowSystem* s_RawMouseOwner = nullptr;
        WNDPROC s_PreviousWndProc = nullptr;
        GLFWwindowfocusfun s_PreviousWindowFocusCallback = nullptr;

        LRESULT CALLBACK RawMouseWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            if (msg == WM_INPUT && s_RawMouseOwner != nullptr)
            {
                UINT size = 0;
                ::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
                if (size > 0)
                {
                    alignas(RAWINPUT) unsigned char stackBuffer[sizeof(RAWINPUT)];
                    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(stackBuffer);
                    std::vector<unsigned char> heapBuffer;
                    if (size > sizeof(stackBuffer))
                    {
                        heapBuffer.resize(size);
                        raw = reinterpret_cast<RAWINPUT*>(heapBuffer.data());
                    }

                    if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER)) == size
                        && raw->header.dwType == RIM_TYPEMOUSE
                        && (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
                    {
                        s_RawMouseOwner->AccumulateRelativeMouseDelta(
                            static_cast<float>(raw->data.mouse.lLastX),
                            static_cast<float>(raw->data.mouse.lLastY));
                    }
                }
            }

            // This WndProc exists exactly while raw mouse capture is registered, i.e. while an OS-level
            // constraint is applied; a focus loss here must revoke it immediately instead of waiting for
            // the GLFW event pump. The regaining side is handled by the GLFW callback.
            if ((msg == WM_ACTIVATE || msg == WM_ACTIVATEAPP) && s_RawMouseOwner != nullptr)
            {
                const bool bActivating = msg == WM_ACTIVATE ? (LOWORD(wParam) != WA_INACTIVE) : (wParam != 0);
                if (!bActivating)
                {
                    s_RawMouseOwner->OnWindowFocusChanged(false);
                }
            }

            if (s_PreviousWndProc != nullptr)
            {
                return ::CallWindowProcW(s_PreviousWndProc, hwnd, msg, wParam, lParam);
            }
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
        }
    }
#endif

    namespace
    {
        void WindowFocusCallback(GLFWwindow* window, int focused)
        {
            GLFWWindowSystem* windowSystem = static_cast<GLFWWindowSystem*>(glfwGetWindowUserPointer(window));
            if (windowSystem == nullptr)
            {
                return;
            }

            // Single focus entry: revokes immediately on loss, replays the current request on gain.
            windowSystem->OnWindowFocusChanged(focused == GLFW_TRUE);

            // Chain to the previously installed handler (the ImGui GLFW backend may have installed it first):
            // swallowing it would silently drop their focus handling.
            if (s_PreviousWindowFocusCallback != nullptr)
            {
                s_PreviousWindowFocusCallback(window, focused);
            }
        }
    }

    WindowSystem* WindowSystem::s_Instance = nullptr;

    void WindowSystem::SetInstance(WindowSystem* instance)
    {
        s_Instance = instance;
    }

    WindowSystem& WindowSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "WindowSystem is not initialized");
        return *s_Instance;
    }

    bool WindowSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    GLFWWindowSystem::~GLFWWindowSystem()
    {
        Shutdown();
    }

    void GLFWWindowSystem::Initialize()
    {
        if (m_IsInitialized)
        {
            return;
        }

        if (!glfwInit())
        {
            // Initialization failed
            ME_LOG(LogPlatform, Error, "Failed to initialize GLFW");
            return;
        }
        m_IsGlfwInitialized = true;

        const bool useVulkan = RHIBackendSelection::IsVulkan();
        if (useVulkan)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }
        else
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }

        // Create the window (Editor replaces title each frame via UpdateWindowTitle).
        const std::string initialTitle = FormatEditorWindowTitleBase();
        m_Window = glfwCreateWindow(m_Width, m_Height, initialTitle.c_str(), nullptr, nullptr);
        if (!m_Window)
        {
            // Window creation failed
            ME_LOG(LogPlatform, Error, "Failed to create GLFW window");
            glfwTerminate();
            m_IsGlfwInitialized = false;
            return;
        }

        if (useVulkan)
        {
            m_HasOpenGLContext = false;
            glfwSetWindowUserPointer(m_Window, this);
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            SetupWindowEventCallbacks();
            ME_LOG(LogPlatform, Info, "GLFW Window Initialized (Vulkan / NO_API)");
            m_IsInitialized = true;
            return;
        }

        m_HasOpenGLContext = true;
        glfwMakeContextCurrent(m_Window);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            // GLAD initialization failed
            ME_LOG(LogPlatform, Error, "Failed to initialize GLAD");
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
            glfwTerminate();
            m_IsGlfwInitialized = false;
            return;
        }

        // Set the viewport
        glViewport(0, 0, m_Width, m_Height);

        glfwSetWindowUserPointer(m_Window, this);
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        SetupWindowEventCallbacks();

        RegisterOnWindowSizeCallback([this](int width, int height)
        {
            glViewport(0, 0, width, height);
        });

        ME_LOG(LogPlatform, Info, "GLFW Window Initialized (OpenGL 4.6)");
        m_IsInitialized = true;
    }

    // Shutdown and clean up resources
    void GLFWWindowSystem::Shutdown()
    {
        if (!m_IsInitialized && !m_IsGlfwInitialized && m_Window == nullptr)
        {
            return;
        }

        // Second line of defence: release the OS-level constraint unconditionally before the
        // window goes away; never rely on the editor having cleared it first.
        m_PointerConstraint.ReleaseAllAndForgetRequests();

        if (m_Window != nullptr && s_PreviousWindowFocusCallback != nullptr)
        {
            glfwSetWindowFocusCallback(m_Window, s_PreviousWindowFocusCallback);
            s_PreviousWindowFocusCallback = nullptr;
        }

        if (m_Window)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }
        
        if (m_IsGlfwInitialized)
        {
            glfwTerminate();
            m_IsGlfwInitialized = false;
            ME_LOG(LogPlatform, Info, "GLFW Terminated");
        }

        m_IsInitialized = false;
    }

    // Check if the window should close
    bool GLFWWindowSystem::ShouldClose() const { return glfwWindowShouldClose(m_Window); }

    void GLFWWindowSystem::Close()
    {
        glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    void GLFWWindowSystem::SetTitle(const char *title) { glfwSetWindowTitle(m_Window, title);}

    void GLFWWindowSystem::SetClearColor(Vector3 color)
    {
        if (!m_HasOpenGLContext)
        {
            return;
        }
        glClearColor(color.x, color.y, color.z, 1.0f);
    }

    // Clear the window
    void GLFWWindowSystem::Clear()
    {
        if (!m_HasOpenGLContext)
        {
            return;
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    // Swap the front and back buffers
    void GLFWWindowSystem::SwapBuffers()
    {
        if (!m_HasOpenGLContext || m_Window == nullptr)
        {
            return;
        }
        glfwSwapBuffers(m_Window);
    }

    // Poll events
    void GLFWWindowSystem::PollEvents() const
    {
        glfwPollEvents();

        // Platform focus truth, sampled every frame: never ImGui's AppFocusLost.
        // A missed focus-lost event is caught here by the watchdog (requirement 6).
        const bool bPlatformActive = m_Window != nullptr && glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE;
        m_PointerConstraint.SyncPlatformFocus(bPlatformActive);
    }

    void GLFWWindowSystem::SetCursorVisible(bool visible)
    {
        // Recorded only: the constraint is applied while this window is active and replayed on focus gain
        // Callers keep their existing per-frame call pattern.
        m_PointerConstraint.SetCursorVisibleRequest(visible);
    }

    void GLFWWindowSystem::SetCursorClipRect(const WindowCursorClipRect& rect)
    {
        m_PointerConstraint.SetClipRectRequest(rect);
    }

    WindowCursorConstraintState GLFWWindowSystem::GetCursorConstraintState() const
    {
        WindowCursorConstraintState state;
        state.bWindowActive = m_PointerConstraint.GetDiagnostics().bWindowActive;

        if (m_Window != nullptr)
        {
            const int cursorMode = glfwGetInputMode(m_Window, GLFW_CURSOR);
            if (cursorMode == GLFW_CURSOR_DISABLED)
            {
                state.NativeCursorMode = EWindowCursorMode::Disabled;
            }
            else if (cursorMode == GLFW_CURSOR_HIDDEN)
            {
                state.NativeCursorMode = EWindowCursorMode::Hidden;
            }
            else
            {
                state.NativeCursorMode = EWindowCursorMode::Normal;
            }
        }

#if defined(_WIN32)
        WindowCursorClipRect osClipRect;
        bool bClipActive = false;
        if (PointerConstraintOs::ReadAppliedClipState(bClipActive, osClipRect))
        {
            // Applied state comes from the OS readback, never from a cached flag: "no clip" is the whole
            // desktop being reachable, not "we think we released it".
            state.bObservable = true;
            state.bClipActive = bClipActive;
            state.ClipRect = osClipRect;
        }
        state.bRawCaptureActive = m_bRawMouseRegistered;
#endif
        return state;
    }

    void GLFWWindowSystem::OnWindowFocusChanged(bool focused)
    {
        m_PointerConstraint.OnWindowFocusChanged(focused);

        for (const auto& callback : m_OnWindowFocusCallbacks)
        {
            callback(focused);
        }
    }

    WindowCursorClipRect GLFWWindowSystem::GetClientScreenRect() const
    {
        WindowCursorClipRect rect;
#if defined(_WIN32)
        if (m_Window == nullptr)
        {
            return rect;
        }

        HWND hwnd = glfwGetWin32Window(m_Window);
        if (hwnd == nullptr)
        {
            return rect;
        }

        RECT clientRect{};
        if (::GetClientRect(hwnd, &clientRect) == FALSE)
        {
            return rect;
        }

        POINT topLeft{clientRect.left, clientRect.top};
        POINT bottomRight{clientRect.right, clientRect.bottom};
        if (::ClientToScreen(hwnd, &topLeft) == FALSE || ::ClientToScreen(hwnd, &bottomRight) == FALSE)
        {
            return rect;
        }

        rect.bEnabled = true;
        rect.Left = topLeft.x;
        rect.Top = topLeft.y;
        rect.Right = bottomRight.x;
        rect.Bottom = bottomRight.y;
#endif
        return rect;
    }

    void GLFWWindowSystem::ApplyOsClip(const WindowCursorClipRect& rect)
    {
#if defined(_WIN32)
        const bool bApplied = PointerConstraintOs::ApplyOsClipRect(rect);
        if (rect.bEnabled && !bApplied)
        {
            ME_LOG(LogPlatform, Warn, "ClipCursor rejected the requested clip rect; the cursor stays unconstrained.");
        }
#else
        (void)rect;
        // Non-Win32: no ClipCursor equivalent wired yet (MVP is Win32-first); focus scoping still applies
        // to the cursor mode and raw capture below.
#endif
    }

    void GLFWWindowSystem::ApplyCursorMode(EWindowCursorMode mode)
    {
        if (m_Window == nullptr)
        {
            return;
        }

        switch (mode)
        {
        case EWindowCursorMode::Hidden:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            break;
        case EWindowCursorMode::Disabled:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            break;
        case EWindowCursorMode::Normal:
        default:
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        }
    }

    void GLFWWindowSystem::ApplyRelativeCapture(bool enabled)
    {
        SetRelativeMouseCaptureEnabled(enabled);
        SyncMouseIntentPreserveWithClip(enabled);
        if (!enabled)
        {
            m_PendingRelativeMouseDelta = Vector2(0.0f, 0.0f);
        }
    }

    Vector2 GLFWWindowSystem::ConsumeRelativeMouseDelta()
    {
        const Vector2 delta = m_PendingRelativeMouseDelta;
        m_PendingRelativeMouseDelta = Vector2(0.0f, 0.0f);
        return delta;
    }

    void GLFWWindowSystem::AccumulateRelativeMouseDelta(float deltaX, float deltaY)
    {
        m_PendingRelativeMouseDelta.x += deltaX;
        m_PendingRelativeMouseDelta.y += deltaY;
    }

    void GLFWWindowSystem::SetRelativeMouseCaptureEnabled(bool enabled)
    {
#if defined(_WIN32)
        if (m_Window == nullptr)
        {
            return;
        }

        HWND hwnd = glfwGetWin32Window(m_Window);
        if (hwnd == nullptr)
        {
            return;
        }

        if (enabled)
        {
            if (!m_bRawMouseRegistered)
            {
                RAWINPUTDEVICE rid{};
                rid.usUsagePage = 0x01;
                rid.usUsage = 0x02; // mouse
                rid.dwFlags = 0;
                rid.hwndTarget = hwnd;
                if (::RegisterRawInputDevices(&rid, 1, sizeof(rid)))
                {
                    m_bRawMouseRegistered = true;
                    s_RawMouseOwner = this;
                    if (s_PreviousWndProc == nullptr)
                    {
                        s_PreviousWndProc = reinterpret_cast<WNDPROC>(
                            ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&RawMouseWndProc)));
                    }
                }
            }
            return;
        }

        if (m_bRawMouseRegistered)
        {
            RAWINPUTDEVICE rid{};
            rid.usUsagePage = 0x01;
            rid.usUsage = 0x02;
            rid.dwFlags = RIDEV_REMOVE;
            rid.hwndTarget = nullptr;
            ::RegisterRawInputDevices(&rid, 1, sizeof(rid));
            m_bRawMouseRegistered = false;
        }

        if (s_RawMouseOwner == this)
        {
            if (s_PreviousWndProc != nullptr)
            {
                ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_PreviousWndProc));
                s_PreviousWndProc = nullptr;
            }
            s_RawMouseOwner = nullptr;
        }
#else
        (void)enabled;
#endif
    }

    void GLFWWindowSystem::SyncMouseIntentPreserveWithClip(bool clipActive)
    {
        if (!InputSystem::HasInstance())
        {
            return;
        }

        InputSystem::Get().SetPreserveMouseIntentBeyondClip(clipActive);
    }

    void GLFWWindowSystem::OnKey(InputKey key, int scancode, InputKeyAction action, int mods)
    {
        for (const auto& callback : m_OnKeyCallbacks)
        {
            callback(key, scancode, action, mods);
        }
    }

    void GLFWWindowSystem::OnMouseButton(InputKey key, InputKeyAction action, int mods)
    {
        for (const auto& callback : m_OnMouseButtonCallbacks)
        {
            callback(key, action, mods);
        }
    }

    void GLFWWindowSystem::OnCursorPos(double xPos, double yPos)
    {
        for (const auto& callback : m_OnCursorPosCallbacks)
        {
            callback(xPos, yPos);
        }
    }

    void GLFWWindowSystem::OnMouseScroll(double xOffset, double yOffset)
    {
        for (const auto& callback : m_OnMouseScrollCallbacks)
        {
            callback(xOffset, yOffset);
        }
    }

    void GLFWWindowSystem::OnWindowSize(int width, int height)
    {
        m_Width = width;
        m_Height = height;

        for (const auto& callback : m_OnWindowSizeCallbacks)
        {
            callback(width, height);
        }
    }

    // Set GLFW callbacks
    void GLFWWindowSystem::SetupWindowEventCallbacks()
    {
        // Bind the static callback functions to GLFW
        glfwSetKeyCallback(m_Window, KeyCallback);
        glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
        glfwSetCursorPosCallback(m_Window, CursorPosCallback);
        glfwSetScrollCallback(m_Window, MouseScrollCallback);
        glfwSetWindowSizeCallback(m_Window, WindowSizeCallback);
        // Keep the previously installed handler (ImGui GLFW backend installs one) and chain to it.
        s_PreviousWindowFocusCallback = glfwSetWindowFocusCallback(m_Window, WindowFocusCallback);

        // Record the focus truth at creation without applying a request: the window keeps the cursor mode
        // Initialize() set, so create-time behaviour is unchanged by this recording.
        m_PointerConstraint.InitializeWindowActive(glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) == GLFW_TRUE);
    }

    bool GLFWWindowSystem::TryConvertGLFWKeyToInputKey(int glfwKey, const InputKey*& outKey)
    {
        outKey = nullptr;
        switch (glfwKey)
        {
            case GLFW_KEY_A: outKey = &InputKeys::Key_A; break;
            case GLFW_KEY_B: outKey = &InputKeys::Key_B; break;
            case GLFW_KEY_C: outKey = &InputKeys::Key_C; break;
            case GLFW_KEY_D: outKey = &InputKeys::Key_D; break;
            case GLFW_KEY_E: outKey = &InputKeys::Key_E; break;
            case GLFW_KEY_F: outKey = &InputKeys::Key_F; break;
            case GLFW_KEY_G: outKey = &InputKeys::Key_G; break;
            case GLFW_KEY_H: outKey = &InputKeys::Key_H; break;
            case GLFW_KEY_I: outKey = &InputKeys::Key_I; break;
            case GLFW_KEY_J: outKey = &InputKeys::Key_J; break;
            case GLFW_KEY_K: outKey = &InputKeys::Key_K; break;
            case GLFW_KEY_L: outKey = &InputKeys::Key_L; break;
            case GLFW_KEY_M: outKey = &InputKeys::Key_M; break;
            case GLFW_KEY_N: outKey = &InputKeys::Key_N; break;
            case GLFW_KEY_O: outKey = &InputKeys::Key_O; break;
            case GLFW_KEY_P: outKey = &InputKeys::Key_P; break;
            case GLFW_KEY_Q: outKey = &InputKeys::Key_Q; break;
            case GLFW_KEY_R: outKey = &InputKeys::Key_R; break;
            case GLFW_KEY_S: outKey = &InputKeys::Key_S; break;
            case GLFW_KEY_T: outKey = &InputKeys::Key_T; break;
            case GLFW_KEY_U: outKey = &InputKeys::Key_U; break;
            case GLFW_KEY_V: outKey = &InputKeys::Key_V; break;
            case GLFW_KEY_W: outKey = &InputKeys::Key_W; break;
            case GLFW_KEY_X: outKey = &InputKeys::Key_X; break;
            case GLFW_KEY_Y: outKey = &InputKeys::Key_Y; break;
            case GLFW_KEY_Z: outKey = &InputKeys::Key_Z; break;

            case GLFW_KEY_0: outKey = &InputKeys::Key_0; break;
            case GLFW_KEY_1: outKey = &InputKeys::Key_1; break;
            case GLFW_KEY_2: outKey = &InputKeys::Key_2; break;
            case GLFW_KEY_3: outKey = &InputKeys::Key_3; break;
            case GLFW_KEY_4: outKey = &InputKeys::Key_4; break;
            case GLFW_KEY_5: outKey = &InputKeys::Key_5; break;
            case GLFW_KEY_6: outKey = &InputKeys::Key_6; break;
            case GLFW_KEY_7: outKey = &InputKeys::Key_7; break;
            case GLFW_KEY_8: outKey = &InputKeys::Key_8; break;
            case GLFW_KEY_9: outKey = &InputKeys::Key_9; break;

            case GLFW_KEY_F1: outKey = &InputKeys::Key_F1; break;
            case GLFW_KEY_F2: outKey = &InputKeys::Key_F2; break;
            case GLFW_KEY_F3: outKey = &InputKeys::Key_F3; break;
            case GLFW_KEY_F4: outKey = &InputKeys::Key_F4; break;
            case GLFW_KEY_F5: outKey = &InputKeys::Key_F5; break;
            case GLFW_KEY_F6: outKey = &InputKeys::Key_F6; break;
            case GLFW_KEY_F7: outKey = &InputKeys::Key_F7; break;
            case GLFW_KEY_F8: outKey = &InputKeys::Key_F8; break;
            case GLFW_KEY_F9: outKey = &InputKeys::Key_F9; break;
            case GLFW_KEY_F10: outKey = &InputKeys::Key_F10; break;
            case GLFW_KEY_F11: outKey = &InputKeys::Key_F11; break;
            case GLFW_KEY_F12: outKey = &InputKeys::Key_F12; break;
            case GLFW_KEY_F13: outKey = &InputKeys::Key_F13; break;
            case GLFW_KEY_F14: outKey = &InputKeys::Key_F14; break;
            case GLFW_KEY_F15: outKey = &InputKeys::Key_F15; break;
            case GLFW_KEY_F16: outKey = &InputKeys::Key_F16; break;
            case GLFW_KEY_F17: outKey = &InputKeys::Key_F17; break;
            case GLFW_KEY_F18: outKey = &InputKeys::Key_F18; break;
            case GLFW_KEY_F19: outKey = &InputKeys::Key_F19; break;
            case GLFW_KEY_F20: outKey = &InputKeys::Key_F20; break;
            case GLFW_KEY_F21: outKey = &InputKeys::Key_F21; break;
            case GLFW_KEY_F22: outKey = &InputKeys::Key_F22; break;
            case GLFW_KEY_F23: outKey = &InputKeys::Key_F23; break;
            case GLFW_KEY_F24: outKey = &InputKeys::Key_F24; break;

            case GLFW_KEY_UP: outKey = &InputKeys::Key_Up; break;
            case GLFW_KEY_DOWN: outKey = &InputKeys::Key_Down; break;
            case GLFW_KEY_LEFT: outKey = &InputKeys::Key_Left; break;
            case GLFW_KEY_RIGHT: outKey = &InputKeys::Key_Right; break;
            case GLFW_KEY_INSERT: outKey = &InputKeys::Key_Insert; break;
            case GLFW_KEY_DELETE: outKey = &InputKeys::Key_Delete; break;
            case GLFW_KEY_HOME: outKey = &InputKeys::Key_Home; break;
            case GLFW_KEY_END: outKey = &InputKeys::Key_End; break;
            case GLFW_KEY_PAGE_UP: outKey = &InputKeys::Key_PageUp; break;
            case GLFW_KEY_PAGE_DOWN: outKey = &InputKeys::Key_PageDown; break;

            case GLFW_KEY_SPACE: outKey = &InputKeys::Key_Space; break;
            case GLFW_KEY_APOSTROPHE: outKey = &InputKeys::Key_Apostrophe; break;
            case GLFW_KEY_COMMA: outKey = &InputKeys::Key_Comma; break;
            case GLFW_KEY_MINUS: outKey = &InputKeys::Key_Minus; break;
            case GLFW_KEY_PERIOD: outKey = &InputKeys::Key_Period; break;
            case GLFW_KEY_SLASH: outKey = &InputKeys::Key_Slash; break;
            case GLFW_KEY_SEMICOLON: outKey = &InputKeys::Key_Semicolon; break;
            case GLFW_KEY_EQUAL: outKey = &InputKeys::Key_Equal; break;
            case GLFW_KEY_LEFT_BRACKET: outKey = &InputKeys::Key_LeftBracket; break;
            case GLFW_KEY_BACKSLASH: outKey = &InputKeys::Key_Backslash; break;
            case GLFW_KEY_RIGHT_BRACKET: outKey = &InputKeys::Key_RightBracket; break;
            case GLFW_KEY_GRAVE_ACCENT: outKey = &InputKeys::Key_GraveAccent; break;
            case GLFW_KEY_WORLD_1: outKey = &InputKeys::Key_World1; break;
            case GLFW_KEY_WORLD_2: outKey = &InputKeys::Key_World2; break;

            case GLFW_KEY_ENTER: outKey = &InputKeys::Key_Enter; break;
            case GLFW_KEY_ESCAPE: outKey = &InputKeys::Key_Escape; break;
            case GLFW_KEY_TAB: outKey = &InputKeys::Key_Tab; break;
            case GLFW_KEY_BACKSPACE: outKey = &InputKeys::Key_Backspace; break;
            case GLFW_KEY_CAPS_LOCK: outKey = &InputKeys::Key_CapsLock; break;
            case GLFW_KEY_SCROLL_LOCK: outKey = &InputKeys::Key_ScrollLock; break;
            case GLFW_KEY_NUM_LOCK: outKey = &InputKeys::Key_NumLock; break;
            case GLFW_KEY_PRINT_SCREEN: outKey = &InputKeys::Key_PrintScreen; break;
            case GLFW_KEY_PAUSE: outKey = &InputKeys::Key_Pause; break;
            case GLFW_KEY_MENU: outKey = &InputKeys::Key_Menu; break;

            case GLFW_KEY_LEFT_SHIFT: outKey = &InputKeys::Key_LeftShift; break;
            case GLFW_KEY_RIGHT_SHIFT: outKey = &InputKeys::Key_RightShift; break;
            case GLFW_KEY_LEFT_CONTROL: outKey = &InputKeys::Key_LeftControl; break;
            case GLFW_KEY_RIGHT_CONTROL: outKey = &InputKeys::Key_RightControl; break;
            case GLFW_KEY_LEFT_ALT: outKey = &InputKeys::Key_LeftAlt; break;
            case GLFW_KEY_RIGHT_ALT: outKey = &InputKeys::Key_RightAlt; break;
            case GLFW_KEY_LEFT_SUPER: outKey = &InputKeys::Key_LeftSuper; break;
            case GLFW_KEY_RIGHT_SUPER: outKey = &InputKeys::Key_RightSuper; break;

            case GLFW_KEY_KP_0: outKey = &InputKeys::Key_KP_0; break;
            case GLFW_KEY_KP_1: outKey = &InputKeys::Key_KP_1; break;
            case GLFW_KEY_KP_2: outKey = &InputKeys::Key_KP_2; break;
            case GLFW_KEY_KP_3: outKey = &InputKeys::Key_KP_3; break;
            case GLFW_KEY_KP_4: outKey = &InputKeys::Key_KP_4; break;
            case GLFW_KEY_KP_5: outKey = &InputKeys::Key_KP_5; break;
            case GLFW_KEY_KP_6: outKey = &InputKeys::Key_KP_6; break;
            case GLFW_KEY_KP_7: outKey = &InputKeys::Key_KP_7; break;
            case GLFW_KEY_KP_8: outKey = &InputKeys::Key_KP_8; break;
            case GLFW_KEY_KP_9: outKey = &InputKeys::Key_KP_9; break;
            case GLFW_KEY_KP_DECIMAL: outKey = &InputKeys::Key_KP_Decimal; break;
            case GLFW_KEY_KP_DIVIDE: outKey = &InputKeys::Key_KP_Divide; break;
            case GLFW_KEY_KP_MULTIPLY: outKey = &InputKeys::Key_KP_Multiply; break;
            case GLFW_KEY_KP_SUBTRACT: outKey = &InputKeys::Key_KP_Subtract; break;
            case GLFW_KEY_KP_ADD: outKey = &InputKeys::Key_KP_Add; break;
            case GLFW_KEY_KP_ENTER: outKey = &InputKeys::Key_KP_Enter; break;
            case GLFW_KEY_KP_EQUAL: outKey = &InputKeys::Key_KP_Equal; break;

            case GLFW_MOUSE_BUTTON_LEFT: outKey = &InputKeys::Mouse_Left; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: outKey = &InputKeys::Mouse_Middle; break;
            case GLFW_MOUSE_BUTTON_RIGHT: outKey = &InputKeys::Mouse_Right; break;
            case GLFW_MOUSE_BUTTON_4: outKey = &InputKeys::Mouse_4; break;
            case GLFW_MOUSE_BUTTON_5: outKey = &InputKeys::Mouse_5; break;
            case GLFW_MOUSE_BUTTON_6: outKey = &InputKeys::Mouse_6; break;
            case GLFW_MOUSE_BUTTON_7: outKey = &InputKeys::Mouse_7; break;
            case GLFW_MOUSE_BUTTON_8: outKey = &InputKeys::Mouse_8; break;

            default:
                return false;
        }
        return outKey != nullptr;
    }

    InputKeyAction GLFWWindowSystem::ConvertGLFWKeyActionToInputKeyAction(int action)
    {
        switch (action)
        {
            case GLFW_PRESS: return InputKeyAction::Press;
            case GLFW_REPEAT: return InputKeyAction::Repeat;
            case GLFW_RELEASE: return InputKeyAction::Release;
            default: return InputKeyAction::Idle;
        }
    }
}