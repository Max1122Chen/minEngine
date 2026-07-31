#include "LuaBindProbe.h"

namespace minEngine
{
    int32_t LuaBindProbe::s_StaticCounter = 0;

    int32_t LuaBindProbe::Add(int32_t a, int32_t b) const
    {
        return a + b;
    }

    void LuaBindProbe::SetValue(int32_t value)
    {
        m_Value = value;
    }

    int32_t LuaBindProbe::GetValue() const
    {
        return m_Value;
    }

    void LuaBindProbe::ResetStaticCounter()
    {
        s_StaticCounter = 0;
    }

    int32_t LuaBindProbe::GetStaticCounter()
    {
        return s_StaticCounter;
    }

    void LuaBindProbe::IncrementStaticCounter()
    {
        ++s_StaticCounter;
    }
}
