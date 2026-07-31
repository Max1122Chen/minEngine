#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace minEngine
{
    // Lightweight sol2 binding / feasibility probe. No Component, reflection, or math deps.
    class MINENGINE_API LuaBindProbe
    {
    public:
        LuaBindProbe() = default;

        int32_t Add(int32_t a, int32_t b) const;
        void SetValue(int32_t value);
        int32_t GetValue() const;

        static void ResetStaticCounter();
        static int32_t GetStaticCounter();
        static void IncrementStaticCounter();

    private:
        int32_t m_Value = 0;
        static int32_t s_StaticCounter;
    };
}
