#pragma once

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

        virtual void Clear() = 0;

        virtual void SwapBuffers() = 0;
        virtual void PollEvents() const = 0;
        virtual void ProcessInput() const = 0;


    };
}

