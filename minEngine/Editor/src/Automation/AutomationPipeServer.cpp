#include "Automation/AutomationPipeServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstring>

namespace minEngine
{
    AutomationPipeServer::~AutomationPipeServer()
    {
        Close();
    }

    bool AutomationPipeServer::Listen(const std::wstring& pipePath)
    {
        Close();

        m_PipePath = pipePath;
        m_PipeHandle = CreateNamedPipeW(
            pipePath.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            64 * 1024,
            64 * 1024,
            0,
            nullptr);

        if (m_PipeHandle == INVALID_HANDLE_VALUE)
        {
            m_PipeHandle = nullptr;
            return false;
        }

        return true;
    }

    void AutomationPipeServer::Close()
    {
        if (m_PipeHandle != nullptr && m_PipeHandle != INVALID_HANDLE_VALUE)
        {
            if (m_ClientConnected)
            {
                DisconnectNamedPipe(static_cast<HANDLE>(m_PipeHandle));
                m_ClientConnected = false;
            }
            CancelIoEx(static_cast<HANDLE>(m_PipeHandle), nullptr);
            CloseHandle(static_cast<HANDLE>(m_PipeHandle));
        }
        m_PipeHandle = nullptr;
        m_PipePath.clear();
    }

    bool AutomationPipeServer::WaitForClient()
    {
        if (!IsListening())
        {
            return false;
        }

        const BOOL connected = ConnectNamedPipe(static_cast<HANDLE>(m_PipeHandle), nullptr);
        if (connected != FALSE)
        {
            m_ClientConnected = true;
            return true;
        }

        if (GetLastError() == ERROR_PIPE_CONNECTED)
        {
            m_ClientConnected = true;
            return true;
        }

        return false;
    }

    void AutomationPipeServer::DisconnectClient()
    {
        if (!IsListening() || !m_ClientConnected)
        {
            return;
        }

        FlushFileBuffers(static_cast<HANDLE>(m_PipeHandle));
        DisconnectNamedPipe(static_cast<HANDLE>(m_PipeHandle));
        m_ClientConnected = false;
    }

    bool AutomationPipeServer::ReadExact(void* buffer, uint32_t byteCount)
    {
        uint8_t* cursor = static_cast<uint8_t*>(buffer);
        uint32_t remaining = byteCount;
        while (remaining > 0)
        {
            DWORD readCount = 0;
            if (ReadFile(static_cast<HANDLE>(m_PipeHandle), cursor, remaining, &readCount, nullptr) == FALSE
                || readCount == 0)
            {
                return false;
            }
            cursor += readCount;
            remaining -= readCount;
        }
        return true;
    }

    bool AutomationPipeServer::WriteExact(const void* buffer, uint32_t byteCount)
    {
        const uint8_t* cursor = static_cast<const uint8_t*>(buffer);
        uint32_t remaining = byteCount;
        while (remaining > 0)
        {
            DWORD written = 0;
            if (WriteFile(static_cast<HANDLE>(m_PipeHandle), cursor, remaining, &written, nullptr) == FALSE
                || written == 0)
            {
                return false;
            }
            cursor += written;
            remaining -= written;
        }
        return true;
    }

    bool AutomationPipeServer::ReadFrame(std::string& outBody)
    {
        uint32_t length = 0;
        if (!ReadExact(&length, sizeof(length)))
        {
            return false;
        }

        // Guard absurd frames (protocol JSON should stay small in MVP).
        constexpr uint32_t kMaxFrameBytes = 16u * 1024u * 1024u;
        if (length > kMaxFrameBytes)
        {
            return false;
        }

        outBody.assign(length, '\0');
        if (length == 0)
        {
            return true;
        }
        return ReadExact(outBody.data(), length);
    }

    bool AutomationPipeServer::WriteFrame(const std::string& body)
    {
        if (body.size() > 0xffffffffu)
        {
            return false;
        }
        const uint32_t length = static_cast<uint32_t>(body.size());
        if (!WriteExact(&length, sizeof(length)))
        {
            return false;
        }
        if (length == 0)
        {
            return true;
        }
        return WriteExact(body.data(), length);
    }
}
