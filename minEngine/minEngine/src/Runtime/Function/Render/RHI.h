#pragma once

namespace minEngine
{
    class RHI
    {
    public:
        virtual ~RHI() = default;

        virtual void Initialize() = 0;
        virtual void Shutdown() = 0;

        
    };
}