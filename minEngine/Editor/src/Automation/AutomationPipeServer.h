#pragma once

#include <cstdint>
#include <string>

namespace minEngine
{
    /**
     * Windows Named Pipe server: length-prefixed UTF-8 JSON frames
     * (u32 little-endian length + body). One connected client at a time.
     *
     * Uses overlapped IO so InterruptIo / Close can unblock ConnectNamedPipe and ReadFile
     * from another thread (Editor Shutdown must not hang on AutomationHost::Stop).
     */
    class AutomationPipeServer
    {
    public:
        AutomationPipeServer() = default;
        ~AutomationPipeServer();

        AutomationPipeServer(const AutomationPipeServer&) = delete;
        AutomationPipeServer& operator=(const AutomationPipeServer&) = delete;

        bool Listen(const std::wstring& pipePath);

        /**
         * Wake blocked WaitForClient / Read / Write (SetEvent + CancelIoEx + Disconnect).
         * Does not CloseHandle the pipe — call Close() after the IO thread has joined.
         */
        void InterruptIo();

        /** InterruptIo + CloseHandle pipe + destroy sync events. */
        void Close();

        bool IsListening() const { return m_PipeHandle != nullptr; }
        const std::wstring& GetPipePath() const { return m_PipePath; }

        /** Block until a client connects, stop is requested, or connect fails. */
        bool WaitForClient();

        void DisconnectClient();

        bool ReadFrame(std::string& outBody);
        bool WriteFrame(const std::string& body);

    private:
        void* m_PipeHandle = nullptr; // HANDLE
        void* m_StopEvent = nullptr;  // HANDLE (manual-reset)
        void* m_IoEvent = nullptr;    // HANDLE (manual-reset, OVERLAPPED.hEvent)
        std::wstring m_PipePath;
        bool m_ClientConnected = false;

        bool EnsureSyncEvents();
        void DestroySyncEvents();
        /** Wait for ov->hEvent or stop. On success fills outTransferred. */
        bool WaitForOverlappedCompletion(/*OVERLAPPED*/ void* overlapped, unsigned long* outTransferred);

        bool ReadExact(void* buffer, uint32_t byteCount);
        bool WriteExact(const void* buffer, uint32_t byteCount);
    };
}
