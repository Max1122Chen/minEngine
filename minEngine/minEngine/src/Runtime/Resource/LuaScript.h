#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <string>

namespace minEngine
{
    class LuaScriptLoader;

    ME_CLASS()
    class LuaScript : public Asset
    {
        ME_GENERATED_BODY(LuaScript)

    public:
        LuaScript() = default;
        ~LuaScript() override = default;

        const std::string& GetSource() const { return m_Source; }
        void SetSource(std::string source) { m_Source = std::move(source); }
        bool IsValid() const { return !m_Source.empty(); }

    protected:
        friend class LuaScriptLoader;

        std::string m_Source;
    };
}

#include "LuaScript.gen.h"
