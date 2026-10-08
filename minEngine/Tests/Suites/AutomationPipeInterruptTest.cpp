#include "AutomationPipeInterruptTest.h"

#include "Automation/AutomationPipeServer.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace minEngine
{
    namespace
    {
        std::wstring MakeUniquePipePath()
        {
            const DWORD pid = GetCurrentProcessId();
            const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
            return L"\\\\.\\pipe\\minEngine.AutomationPipeInterrupt." + std::to_wstring(pid) + L"."
                + std::to_wstring(static_cast<unsigned long long>(now));
        }

        bool RunInterruptUnblocksWaitForClient()
        {
            AutomationPipeServer server;
            const std::wstring pipePath = MakeUniquePipePath();
            if (!server.Listen(pipePath))
            {
                ME_LOG(LogTest, Error, "AutomationPipeInterrupt: Listen failed.");
                return false;
            }

            std::atomic<bool> waitReturned{false};
            std::thread ioThread(
                [&server, &waitReturned]()
                {
                    (void)server.WaitForClient();
                    waitReturned.store(true);
                });

            // Give the IO thread time to enter overlapped ConnectNamedPipe.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            server.InterruptIo();

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!waitReturned.load() && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (!waitReturned.load())
            {
                ME_LOG(LogTest, Error, "AutomationPipeInterrupt: WaitForClient still blocked after InterruptIo.");
                // Best-effort unblock before join to avoid hanging the test process.
                server.Close();
                if (ioThread.joinable())
                {
                    ioThread.join();
                }
                return false;
            }

            if (ioThread.joinable())
            {
                ioThread.join();
            }
            server.Close();
            return true;
        }
    }

    bool RunAutomationPipeInterruptTests()
    {
        return RunInterruptUnblocksWaitForClient();
    }
}

#include "doctest.h"
#include "EngineTestFixture.h"

TEST_CASE("automation-pipe-interrupt: InterruptIo unblocks WaitForClient [smoke]")
{
    minEngine::EngineReflectionFixture fixture;
    REQUIRE(fixture.IsReflectionReady());
    CHECK(minEngine::RunAutomationPipeInterruptTests());
}
