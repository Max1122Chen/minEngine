#include "MIRGLSLPrinter.h"

namespace minEngine
{
    MIRGLSLPrinter& MIRGLSLPrinter::Append(std::string_view text)
    {
        Buffer.append(text);
        return *this;
    }

    MIRGLSLPrinter& MIRGLSLPrinter::Append(char character)
    {
        Buffer.push_back(character);
        return *this;
    }

    MIRGLSLPrinter& MIRGLSLPrinter::Indent()
    {
        for (int i = 0; i < Tabs; ++i)
        {
            Buffer.append("    ");
        }
        return *this;
    }

    MIRGLSLPrinter& MIRGLSLPrinter::NewLine()
    {
        Buffer.push_back('\n');
        return Indent();
    }

    MIRGLSLPrinter& MIRGLSLPrinter::EndStatement()
    {
        Buffer.append(";\n");
        return *this;
    }

    MIRGLSLPrinter& MIRGLSLPrinter::OpenBrace()
    {
        Buffer.push_back('{');
        ++Tabs;
        return NewLine();
    }

    MIRGLSLPrinter& MIRGLSLPrinter::CloseBrace()
    {
        --Tabs;
        if (!Buffer.empty() && Buffer.back() == ' ')
        {
            Buffer.pop_back();
        }
        Buffer.push_back('}');
        return *this;
    }

    MIRGLSLPrinter& MIRGLSLPrinter::BeginArgs()
    {
        bFirstListItem = true;
        return Append('(');
    }

    MIRGLSLPrinter& MIRGLSLPrinter::EndArgs()
    {
        return Append(')');
    }

    MIRGLSLPrinter& MIRGLSLPrinter::ListSeparator()
    {
        if (!bFirstListItem)
        {
            Append(", ");
        }
        bFirstListItem = false;
        return *this;
    }

    bool MIRGLSLPrinter::EndsWith(char suffix) const
    {
        return !Buffer.empty() && Buffer.back() == suffix;
    }

    std::string MIRGLSLPrinter::TakeBuffer()
    {
        return std::move(Buffer);
    }
}
