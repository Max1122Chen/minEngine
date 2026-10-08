#include "Automation/AutomationPipeServer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstring>

namespace minEngine
{
    namespace
    {
        bool IsStopWait(DWORD waitResult)
        {
            return waitResult == WAIT_OBJECT_0 + 1;
        }
    }

    AutomationPipeServer::~AutomationPipeServer()
    {
        Close();
    }

    bool AutomationPipeServer::EnsureSyncEvents()
    {
        if (m_StopEvent == nullptr)
        {
            m_StopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (m_StopEvent == nullptr)
            {
                return false;
            }
        }

        if (m_IoEvent == nullptr)
        {
            m_IoEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (m_IoEvent == nullptr)
            {
                return false;
            }
        }

        ResetEvent(static_cast<HANDLE>(m_StopEvent));
        ResetEvent(static_cast<HANDLE>(m_IoEvent));
        return true;
    }

    void AutomationPipeServer::DestroySyncEvents()
    {
        if (m_IoEvent != nullptr)
        {
            CloseHandle(static_cast<HANDLE>(m_IoEvent));
            m_IoEvent = nullptr;
        }
        if (m_StopEvent != nullptr)
        {
            CloseHandle(static_cast<HANDLE>(m_StopEvent));
            m_StopEvent = nullptr;
        }
    }

    bool AutomationPipeServer::Listen(const std::wstring& pipePath)
    {
        Close();

        if (!EnsureSyncEvents())
        {
            DestroySyncEvents();
            return false;
        }

        m_PipePath = pipePath;
        m_PipeHandle = CreateNamedPipeW(
            pipePath.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            64 * 1024,
            64 * 1024,
            0,
            nullptr);

        if (m_PipeHandle == INVALID_HANDLE_VALUE)
        {
            m_PipeHandle = nullptr;
            DestroySyncEvents();
            m_PipePath.clear();
            return false;
        }

        return true;
    }

    void AutomationPipeServer::InterruptIo()
    {
        if (m_StopEvent != nullptr)
        {
            SetEvent(static_cast<HANDLE>(m_StopEvent));
        }

        // Keep the pipe HANDLE alive until Close() after the IO thread joins so
        // WaitForOverlappedCompletion can CancelIoEx / drain safely.
        if (m_PipeHandle != nullptr && m_PipeHandle != INVALID_HANDLE_VALUE)
        {
            if (m_ClientConnected)
            {
                DisconnectNamedPipe(static_cast<HANDLE>(m_PipeHandle));
                m_ClientConnected = false;
            }
            CancelIoEx(static_cast<HANDLE>(m_PipeHandle), nullptr);
        }
    }

    void AutomationPipeServer::Close()
    {
        InterruptIo();

        if (m_PipeHandle != nullptr && m_PipeHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(static_cast<HANDLE>(m_PipeHandle));
        }
        m_PipeHandle = nullptr;
        DestroySyncEvents();
        m_PipePath.clear();
        m_ClientConnected = false;
    }

    bool AutomationPipeServer::WaitForOverlappedCompletion(void* overlapped, unsigned long* outTransferred)
    {
        OVERLAPPED* ov = static_cast<OVERLAPPED*>(overlapped);
        if (m_PipeHandle == nullptr || m_StopEvent == nullptr || ov == nullptr || ov->hEvent == nullptr)
        {
            return false;
        }

        HANDLE waitHandles[2] = {
            ov->hEvent,
            static_cast<HANDLE>(m_StopEvent),
        };

        const DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (IsStopWait(waitResult))
        {
            CancelIoEx(static_cast<HANDLE>(m_PipeHandle), ov);
            // Drain completion so the OVERLAPPED is not left pending.
            DWORD transferred = 0;
            GetOverlappedResult(static_cast<HANDLE>(m_PipeHandle), ov, &transferred, TRUE);
            return false;
        }

        if (waitResult != WAIT_OBJECT_0)
        {
            return false;
        }

        DWORD transferred = 0;
        if (GetOverlappedResult(static_cast<HANDLE>(m_PipeHandle), ov, &transferred, TRUE) == FALSE)
        {
            return false;
        }

        if (outTransferred != nullptr)
        {
            *outTransferred = transferred;
        }
        return true;
    }

    bool AutomationPipeServer::WaitForClient()
    {
        if (!IsListening() || m_IoEvent == nullptr || m_StopEvent == nullptr)
        {
            return false;
        }

        if (WaitForSingleObject(static_cast<HANDLE>(m_StopEvent), 0) == WAIT_OBJECT_0)
        {
            return false;
        }

        OVERLAPPED overlapped = {};
        overlapped.hEvent = static_cast<HANDLE>(m_IoEvent);
        ResetEvent(overlapped.hEvent);

        const BOOL connected = ConnectNamedPipe(static_cast<HANDLE>(m_PipeHandle), &overlapped);
        if (connected != FALSE)
        {
            m_ClientConnected = true;
            return true;
        }

        const DWORD error = GetLastError();
        if (error == ERROR_PIPE_CONNECTED)
        {
            m_ClientConnected = true;
            return true;
        }

        if (error != ERROR_IO_PENDING)
        {
            return false;
        }

        DWORD transferred = 0;
        if (!WaitForOverlappedCompletion(&overlapped, &transferred))
        {
            return false;
        }

        m_ClientConnected = true;
        return true;
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
        if (!IsListening() || m_IoEvent == nullptr || m_StopEvent == nullptr)
        {
            return false;
        }

        uint8_t* cursor = static_cast<uint8_t*>(buffer);
        uint32_t remaining = byteCount;
        while (remaining > 0)
        {
            if (WaitForSingleObject(static_cast<HANDLE>(m_StopEvent), 0) == WAIT_OBJECT_0)
            {
                return false;
            }

            OVERLAPPED overlapped = {};
            overlapped.hEvent = static_cast<HANDLE>(m_IoEvent);
            ResetEvent(overlapped.hEvent);

            DWORD readCount = 0;
            const BOOL ok = ReadFile(
                static_cast<HANDLE>(m_PipeHandle),
                cursor,
                remaining,
                &readCount,
                &overlapped);

            if (ok != FALSE)
            {
                if (readCount == 0)
                {
                    return false;
                }
                cursor += readCount;
                remaining -= readCount;
                continue;
            }

            const DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING)
            {
                return false;
            }

            if (!WaitForOverlappedCompletion(&overlapped, &readCount) || readCount == 0)
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
        if (!IsListening() || m_IoEvent == nullptr || m_StopEvent == nullptr)
        {
            return false;
        }

        const uint8_t* cursor = static_cast<const uint8_t*>(buffer);
        uint32_t remaining = byteCount;
        while (remaining > 0)
        {
            if (WaitForSingleObject(static_cast<HANDLE>(m_StopEvent), 0) == WAIT_OBJECT_0)
            {
                return false;
            }

            OVERLAPPED overlapped = {};
            overlapped.hEvent = static_cast<HANDLE>(m_IoEvent);
            ResetEvent(overlapped.hEvent);

            DWORD written = 0;
            const BOOL ok = WriteFile(
                static_cast<HANDLE>(m_PipeHandle),
                cursor,
                remaining,
                &written,
                &overlapped);

            if (ok != FALSE)
            {
                if (written == 0)
                {
                    return false;
                }
                cursor += written;
                remaining -= written;
                continue;
            }

            const DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING)
            {
                return false;
            }

            if (!WaitForOverlappedCompletion(&overlapped, &written) || written == 0)
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
