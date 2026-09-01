#pragma once

#include "Runtime/Core/Command/CommandTypes.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace minEngine::Command
{
    struct CommandOutputSegment
    {
        CommandOutputKind Kind = CommandOutputKind::Plain;
        std::string Text;
    };

    struct CommandOutputLine
    {
        std::vector<CommandOutputSegment> Segments;
        bool bSelectable = true;
    };

    struct CommandResult
    {
        CommandStatus Status = CommandStatus::Ok;
        std::string Message;
        std::vector<CommandOutputLine> Lines;

        static CommandResult MakeOk(std::string message = {});
        static CommandResult MakeError(std::string message);
    };

    class CommandOutputBuilder
    {
    public:
        CommandOutputBuilder& AddLine(CommandOutputKind kind, std::string text);
        CommandOutputBuilder& AddSegment(CommandOutputKind kind, std::string text);
        CommandOutputBuilder& NewLine();

        CommandResult BuildOk(std::string message = {}) const;
        CommandResult BuildError(std::string message) const;

        const std::vector<CommandOutputLine>& GetLines() const { return m_Lines; }

        std::string FlattenToPlainText() const;

    private:
        std::vector<CommandOutputLine> m_Lines;
        CommandOutputLine* m_CurrentLine = nullptr;
    };
}
