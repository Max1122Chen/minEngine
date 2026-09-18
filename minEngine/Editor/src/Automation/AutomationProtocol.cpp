#include "Automation/AutomationProtocol.h"

#include <cctype>
#include <charconv>
#include <string>

namespace minEngine
{
    std::string AutomationProtocol::FormatSessionId(uint64_t documentIdValue)
    {
        return "sess-" + std::to_string(documentIdValue);
    }

    bool AutomationProtocol::TryParseSessionId(std::string_view sessionId, uint64_t& outDocumentIdValue)
    {
        constexpr std::string_view kPrefix = "sess-";
        if (sessionId.size() <= kPrefix.size()
            || sessionId.compare(0, kPrefix.size(), kPrefix) != 0)
        {
            return false;
        }

        const std::string_view digits = sessionId.substr(kPrefix.size());
        uint64_t value = 0;
        const char* begin = digits.data();
        const char* end = begin + digits.size();
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end)
        {
            return false;
        }

        outDocumentIdValue = value;
        return true;
    }

    std::wstring AutomationProtocol::NormalizePipePath(std::string_view pipeName)
    {
        std::wstring wide(pipeName.begin(), pipeName.end());
        constexpr std::wstring_view kPrefix = L"\\\\.\\pipe\\";
        if (wide.rfind(kPrefix, 0) == 0)
        {
            return wide;
        }
        return std::wstring(kPrefix) + wide;
    }

    std::string AutomationProtocol::NarrowPipePath(const std::wstring& pipePath)
    {
        std::string narrow;
        narrow.reserve(pipePath.size());
        for (wchar_t ch : pipePath)
        {
            narrow.push_back(static_cast<char>(ch <= 0x7f ? ch : '?'));
        }
        return narrow;
    }
}
