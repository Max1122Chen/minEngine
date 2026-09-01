#include "Runtime/Core/Command/CommandResult.h"

namespace minEngine::Command
{
    CommandResult CommandResult::MakeOk(std::string message)
    {
        CommandResult result;
        result.Status = CommandStatus::Ok;
        result.Message = std::move(message);
        return result;
    }

    CommandResult CommandResult::MakeError(std::string message)
    {
        CommandResult result;
        result.Status = CommandStatus::Error;
        result.Message = std::move(message);
        return result;
    }

    CommandOutputBuilder& CommandOutputBuilder::AddLine(CommandOutputKind kind, std::string text)
    {
        CommandOutputLine line;
        line.Segments.push_back(CommandOutputSegment{kind, std::move(text)});
        m_Lines.push_back(std::move(line));
        m_CurrentLine = nullptr;
        return *this;
    }

    CommandOutputBuilder& CommandOutputBuilder::AddSegment(CommandOutputKind kind, std::string text)
    {
        if (m_CurrentLine == nullptr)
        {
            m_Lines.emplace_back();
            m_CurrentLine = &m_Lines.back();
        }

        m_CurrentLine->Segments.push_back(CommandOutputSegment{kind, std::move(text)});
        return *this;
    }

    CommandOutputBuilder& CommandOutputBuilder::NewLine()
    {
        m_CurrentLine = nullptr;
        return *this;
    }

    CommandResult CommandOutputBuilder::BuildOk(std::string message) const
    {
        CommandResult result = CommandResult::MakeOk(std::move(message));
        result.Lines = m_Lines;
        if (result.Message.empty())
        {
            result.Message = FlattenToPlainText();
        }
        return result;
    }

    CommandResult CommandOutputBuilder::BuildError(std::string message) const
    {
        CommandResult result = CommandResult::MakeError(std::move(message));
        result.Lines = m_Lines;
        if (result.Message.empty())
        {
            result.Message = FlattenToPlainText();
        }
        return result;
    }

    std::string CommandOutputBuilder::FlattenToPlainText() const
    {
        std::string flattened;
        for (size_t lineIndex = 0; lineIndex < m_Lines.size(); ++lineIndex)
        {
            if (lineIndex > 0)
            {
                flattened.push_back('\n');
            }

            const CommandOutputLine& line = m_Lines[lineIndex];
            for (size_t segmentIndex = 0; segmentIndex < line.Segments.size(); ++segmentIndex)
            {
                flattened += line.Segments[segmentIndex].Text;
            }
        }
        return flattened;
    }
}
