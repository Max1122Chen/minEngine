#include "Runtime/Function/Pointer/WindowSystemPointerModeSink.h"

#include "Runtime/Function/Render/WindowSystem.h"

namespace minEngine
{
    WindowSystemPointerModeSink::WindowSystemPointerModeSink(WindowSystem& window)
        : m_Window(window)
    {
    }

    void WindowSystemPointerModeSink::Apply(const PointerWindowCommand& command)
    {
        WindowCursorClipRect clip = command.ClipRect;
        clip.bEnabled = command.bClip;
        m_Window.ApplyPointerWindowCommand(command.CursorMode, clip, command.bRelativeCapture);
    }
}
