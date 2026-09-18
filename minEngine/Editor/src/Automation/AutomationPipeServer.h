#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    /**
     * Windows Named Pipe server: length-prefixed UTF-8 JSON frames
     * (u32 little-endian length + body). One connected client at a time.
     */
    class AutomationPipeServer
    {
    public:
        AutomationPipeServer() = default;
        ~AutomationPipeServer();

        AutomationPipeServer(const AutomationPipeServer&) = delete;
        AutomationPipeServer& operator=(const AutomationPipeServer&) = delete;

        bool Listen(const std::wstring& pipePath);
        void Close();

        bool IsListening() const { return m_PipeHandle != nullptr; }
        const std::wstring& GetPipePath() const { return m_PipePath; }

        /** Block until a client connects or stop is requested via CancelIoEx / Close. */
        bool WaitForClient();

        void DisconnectClient();

        bool ReadFrame(std::string& outBody);
        bool WriteFrame(const std::string& body);

    private:
        void* m_PipeHandle = nullptr; // HANDLE
        std::wstring m_PipePath;
        bool m_ClientConnected = false;

        bool ReadExact(void* buffer, uint32_t byteCount);
        bool WriteExact(const void* buffer, uint32_t byteCount);
    };
}
