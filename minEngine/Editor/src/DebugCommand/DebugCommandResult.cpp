#include "DebugCommand/DebugCommandResult.h"

namespace minEngine::DebugCommand
{
    DebugCommandResult DebugCommandResult::MakeOk(std::string message)
    {
        DebugCommandResult result;
        result.Status = DebugCommandStatus::Ok;
        result.Message = std::move(message);
        return result;
    }

    DebugCommandResult DebugCommandResult::MakeError(std::string message)
    {
        DebugCommandResult result;
        result.Status = DebugCommandStatus::Error;
        result.Message = std::move(message);
        return result;
    }

    DebugCommandOutputBuilder& DebugCommandOutputBuilder::AddLine(DebugCommandOutputKind kind, std::string text)
    {
        DebugCommandOutputLine line;
        line.Segments.push_back(DebugCommandOutputSegment{kind, std::move(text)});
        m_Lines.push_back(std::move(line));
        m_CurrentLine = nullptr;
        return *this;
    }

    DebugCommandOutputBuilder& DebugCommandOutputBuilder::AddSegment(DebugCommandOutputKind kind, std::string text)
    {
        if (m_CurrentLine == nullptr)
        {
            m_Lines.emplace_back();
            m_CurrentLine = &m_Lines.back();
        }

        m_CurrentLine->Segments.push_back(DebugCommandOutputSegment{kind, std::move(text)});
        return *this;
    }

    DebugCommandOutputBuilder& DebugCommandOutputBuilder::NewLine()
    {
        m_CurrentLine = nullptr;
        return *this;
    }

    DebugCommandOutputBuilder& DebugCommandOutputBuilder::SetPayloadJson(std::string payloadJson)
    {
        m_PayloadJson = std::move(payloadJson);
        return *this;
    }

    DebugCommandResult DebugCommandOutputBuilder::BuildOk(std::string message) const
    {
        DebugCommandResult result = DebugCommandResult::MakeOk(std::move(message));
        result.Lines = m_Lines;
        result.PayloadJson = m_PayloadJson;
        if (result.Message.empty())
        {
            result.Message = FlattenToPlainText();
        }
        return result;
    }

    DebugCommandResult DebugCommandOutputBuilder::BuildError(std::string message) const
    {
        DebugCommandResult result = DebugCommandResult::MakeError(std::move(message));
        result.Lines = m_Lines;
        result.PayloadJson = m_PayloadJson;
        if (result.Message.empty())
        {
            result.Message = FlattenToPlainText();
        }
        return result;
    }

    std::string DebugCommandOutputBuilder::FlattenToPlainText() const
    {
        std::string flattened;
        for (size_t lineIndex = 0; lineIndex < m_Lines.size(); ++lineIndex)
        {
            if (lineIndex > 0)
            {
                flattened.push_back('\n');
            }

            const DebugCommandOutputLine& line = m_Lines[lineIndex];
            for (size_t segmentIndex = 0; segmentIndex < line.Segments.size(); ++segmentIndex)
            {
                flattened += line.Segments[segmentIndex].Text;
            }
        }
        return flattened;
    }
}
