#pragma once

namespace minEngine
{
    class RHI
    {
    public:

        virtual void Initialize() = 0;

        virtual void Shutdown() = 0;

        virtual ~RHI() = default;
                 
        
    };
}