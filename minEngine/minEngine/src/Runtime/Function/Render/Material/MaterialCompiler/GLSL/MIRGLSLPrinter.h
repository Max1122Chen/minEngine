#pragma once

#include "Core.h"
#include <string>

namespace minEngine
{
    // Lightweight HLSL/GLSL emission buffer modeled after UE FHLSLPrinter.
    struct MIRGLSLPrinter
    {
        std::string Buffer;
        int Tabs = 0;
        bool bFirstListItem = true;

        MIRGLSLPrinter& Append(std::string_view text);
        MIRGLSLPrinter& Append(char character);
        MIRGLSLPrinter& NewLine();
        MIRGLSLPrinter& Indent();
        MIRGLSLPrinter& EndStatement();
        MIRGLSLPrinter& OpenBrace();
        MIRGLSLPrinter& CloseBrace();
        MIRGLSLPrinter& BeginArgs();
        MIRGLSLPrinter& EndArgs();
        MIRGLSLPrinter& ListSeparator();

        bool EndsWith(char suffix) const;
        std::string TakeBuffer();
    };
}
