#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace minEngine
{
    /** Wire protocolVersion for schemas/automation/v1. */
    inline constexpr int kAutomationProtocolVersion = 1;

    class AutomationProtocol
    {
    public:
        static std::string FormatSessionId(uint64_t documentIdValue);
        static bool TryParseSessionId(std::string_view sessionId, uint64_t& outDocumentIdValue);
        static std::wstring NormalizePipePath(std::string_view pipeName);
        static std::string NarrowPipePath(const std::wstring& pipePath);
    };
}
