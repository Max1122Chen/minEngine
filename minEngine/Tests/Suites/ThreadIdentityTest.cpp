#include "Runtime/Core/Threading/ThreadIdentity.h"

#include "doctest.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <cstdint>
#include <string>
#include <thread>

using minEngine::CaptureCurrentThreadIdentity;
using minEngine::CaptureDiagnosticFrames;
using minEngine::FormatAddressForLog;
using minEngine::ThreadIdentity;

namespace
{
    void LocalFunctionForNaming()
    {
    }

    std::string FormatAddress(const void* address)
    {
        char buffer[320]{};
        FormatAddressForLog(buffer, sizeof(buffer), address);
        return std::string(buffer);
    }
}

TEST_CASE("frame-threading: thread identity names this thread and its role [smoke]")
{
    const ThreadIdentity identity = CaptureCurrentThreadIdentity();
#if defined(_WIN32)
    CHECK(identity.NativeThreadId == static_cast<std::uint32_t>(::GetCurrentThreadId()));
#else
    CHECK(identity.NativeThreadId == 0u);
#endif
    CHECK(identity.RoleName != nullptr);
    CHECK(std::string(identity.RoleName).empty() == false);

    std::uint32_t otherThreadId = 0;
    std::thread worker([&otherThreadId]() {
        otherThreadId = CaptureCurrentThreadIdentity().LogThreadId;
    });
    worker.join();

    // The log id is what a reader greps for, so two live threads must not share one.
    CHECK(otherThreadId != identity.LogThreadId);
}

TEST_CASE("frame-threading: a diagnostic address names its image and offset [smoke]")
{
    // An offset into a named image is symbolisable after the fact; a bare address is not,
    // because the load base changes every run.
    const std::string text = FormatAddress(reinterpret_cast<const void*>(&LocalFunctionForNaming));
    CHECK(text.find("+0x") != std::string::npos);
    CHECK(text.find(".exe+0x") != std::string::npos);
}

TEST_CASE("frame-threading: diagnostic frames start at the reporting caller [smoke]")
{
    void* frames[minEngine::kDiagnosticFrameCount]{};
    const std::size_t frameCount = CaptureDiagnosticFrames(frames, minEngine::kDiagnosticFrameCount);
#if defined(_WIN32)
    CHECK(frameCount > 0);
    CHECK(frameCount <= minEngine::kDiagnosticFrameCount);
#else
    CHECK(frameCount == 0);
#endif
}
