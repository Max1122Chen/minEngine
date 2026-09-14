#pragma once

#include "DebugCommand/DebugCommandTypes.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minEngine::DebugCommand
{
    struct DebugCommandOutputSegment
    {
        DebugCommandOutputKind Kind = DebugCommandOutputKind::Plain;
        std::string Text;
    };

    struct DebugCommandOutputLine
    {
        std::vector<DebugCommandOutputSegment> Segments;
        bool bSelectable = true;
    };

    struct DebugCommandResult
    {
        DebugCommandStatus Status = DebugCommandStatus::Ok;
        std::string Message;
        std::vector<DebugCommandOutputLine> Lines;
        // Machine-readable JSON object/array (ED-F12). Empty when not provided.
        // Headless/Agent consumers must prefer this over parsing Lines.
        std::string PayloadJson;

        static DebugCommandResult MakeOk(std::string message = {});
        static DebugCommandResult MakeError(std::string message);
    };

    class DebugCommandOutputBuilder
    {
    public:
        DebugCommandOutputBuilder& AddLine(DebugCommandOutputKind kind, std::string text);
        DebugCommandOutputBuilder& AddSegment(DebugCommandOutputKind kind, std::string text);
        DebugCommandOutputBuilder& NewLine();
        DebugCommandOutputBuilder& SetPayloadJson(std::string payloadJson);

        DebugCommandResult BuildOk(std::string message = {}) const;
        DebugCommandResult BuildError(std::string message) const;

        const std::vector<DebugCommandOutputLine>& GetLines() const { return m_Lines; }

        std::string FlattenToPlainText() const;

    private:
        std::vector<DebugCommandOutputLine> m_Lines;
        DebugCommandOutputLine* m_CurrentLine = nullptr;
        std::string m_PayloadJson;
    };
}
