#include "Runtime/Core/Reflection/ReflectionDisplayNames.h"

#include "Runtime/Core/Reflection/MEProperties.h"

namespace minEngine::Reflection
{
    namespace
    {
        bool IsUpperAscii(char character)
        {
            return character >= 'A' && character <= 'Z';
        }

        bool IsLowerAscii(char character)
        {
            return character >= 'a' && character <= 'z';
        }

        bool IsAlphaAscii(char character)
        {
            return IsUpperAscii(character) || IsLowerAscii(character);
        }

        bool ShouldStripMemberPrefix(std::string_view memberName)
        {
            if (memberName.size() < 3)
            {
                return false;
            }

            const char prefix = memberName[0];
            if (prefix != 'm' && prefix != 'x' && prefix != 'b')
            {
                return false;
            }

            if (memberName[1] != '_')
            {
                return false;
            }

            return IsAlphaAscii(memberName[2]);
        }

        std::string_view StripMemberPrefix(std::string_view memberName)
        {
            if (!ShouldStripMemberPrefix(memberName))
            {
                return memberName;
            }

            return memberName.substr(2);
        }

        bool ShouldInsertWordBreak(std::string_view text, size_t index)
        {
            if (index == 0 || index >= text.size())
            {
                return false;
            }

            const char current = text[index];
            if (!IsUpperAscii(current))
            {
                return false;
            }

            const char previous = text[index - 1];
            if (IsLowerAscii(previous))
            {
                return true;
            }

            if (!IsUpperAscii(previous))
            {
                return false;
            }

            if (index + 1 >= text.size())
            {
                return false;
            }

            return IsLowerAscii(text[index + 1]);
        }

        std::string InsertCamelCaseWordBreaks(std::string_view text)
        {
            if (text.empty())
            {
                return {};
            }

            std::string result;
            result.reserve(text.size() + 8);

            for (size_t index = 0; index < text.size(); ++index)
            {
                if (ShouldInsertWordBreak(text, index))
                {
                    result.push_back(' ');
                }

                result.push_back(text[index]);
            }

            return result;
        }
    }

    std::string FormatMemberDisplayName(std::string_view memberName)
    {
        const std::string_view withoutPrefix = StripMemberPrefix(memberName);
        return InsertCamelCaseWordBreaks(withoutPrefix);
    }

    const char* GetPropertyDisplayName(const MEProperty& property)
    {
        if (const std::string* displayName = property.FindMetadata("DisplayName"))
        {
            if (!displayName->empty())
            {
                return displayName->c_str();
            }
        }

        thread_local std::string formattedDisplayName;
        formattedDisplayName = FormatMemberDisplayName(property.GetName());
        return formattedDisplayName.c_str();
    }
}
