#pragma once

#include "Runtime/Core/Threading/FrameThreading.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace minEngine
{
    /**
     * Who is asking. A report about a native object is unreadable without it: the same call is correct
     * on the thread that owns the graphics context and a defect on any other, so "which thread" is the
     * first thing a reader needs and the reason these records exist.
     *
     * LogThreadId repeats the value LogSystem stamps on every record, so every line a thread emitted
     * can be pulled out of the log with one search.
     */
    struct ThreadIdentity
    {
        std::uint32_t LogThreadId = 0;
        std::uint32_t NativeThreadId = 0;
        const char* RoleName = "none";
    };

    inline const char* FrameThreadRoleName(const FrameThreading& frameThreading, std::thread::id threadId)
    {
        // Single mode binds all three kinds to one OS thread, so the answer is a set, not a label.
        const bool isGame = frameThreading.GetThreadId(FrameThreadKind::Game) == threadId;
        const bool isRender = frameThreading.GetThreadId(FrameThreadKind::Render) == threadId;
        const bool isRhi = frameThreading.GetThreadId(FrameThreadKind::RHI) == threadId;

        if (isGame && isRender && isRhi)
        {
            return "Game+Render+RHI";
        }
        if (isRender && isRhi)
        {
            return "Render+RHI";
        }
        if (isGame && isRender)
        {
            return "Game+Render";
        }
        if (isGame && isRhi)
        {
            return "Game+RHI";
        }
        if (isGame)
        {
            return "Game";
        }
        if (isRender)
        {
            return "Render";
        }
        if (isRhi)
        {
            return "RHI";
        }
        return "none (not a frame role thread)";
    }

    inline ThreadIdentity CaptureCurrentThreadIdentity()
    {
        ThreadIdentity identity{};
        const std::thread::id self = std::this_thread::get_id();
        identity.LogThreadId = static_cast<std::uint32_t>(std::hash<std::thread::id>{}(self));
#if defined(_WIN32)
        identity.NativeThreadId = static_cast<std::uint32_t>(GetCurrentThreadId());
#endif
        if (!FrameThreading::HasInstance())
        {
            identity.RoleName = "unbound (FrameThreading not initialized)";
            return identity;
        }
        identity.RoleName = FrameThreadRoleName(FrameThreading::Get(), self);
        return identity;
    }

    /** Frames kept for a diagnostic record: enough to name the caller and its parent, no more. */
    constexpr std::size_t kDiagnosticFrameCount = 10;

    inline std::size_t CaptureDiagnosticFrames(void** frames, std::size_t frameCapacity)
    {
#if defined(_WIN32)
        if (frames == nullptr || frameCapacity == 0)
        {
            return 0;
        }
        // Skip this helper itself so the first reported frame is the caller.
        return CaptureStackBackTrace(1, static_cast<DWORD>(frameCapacity), frames, nullptr);
#else
        (void)frames;
        (void)frameCapacity;
        return 0;
#endif
    }

    /**
     * An absolute address is not symbolisable on its own: every reader needs the module it falls in and
     * the offset into it, because the load base changes per run. Write the address in that form.
     */
    inline void FormatAddressForLog(char* buffer, std::size_t bufferSize, const void* address)
    {
        if (buffer == nullptr || bufferSize == 0)
        {
            return;
        }
#if defined(_WIN32)
        HMODULE module = nullptr;
        if (address != nullptr
            && GetModuleHandleExA(
                   GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                   static_cast<LPCSTR>(address),
                   &module)
            && module != nullptr)
        {
            char modulePath[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(module, modulePath, MAX_PATH);
            if (length != 0 && length < MAX_PATH)
            {
                const char* fileName = modulePath;
                for (const char* cursor = modulePath; *cursor != '\0'; ++cursor)
                {
                    if (*cursor == '\\' || *cursor == '/')
                    {
                        fileName = cursor + 1;
                    }
                }
                const auto base = reinterpret_cast<std::uintptr_t>(module);
                const auto addr = reinterpret_cast<std::uintptr_t>(address);
                std::snprintf(
                    buffer,
                    bufferSize,
                    "%s+0x%llX",
                    fileName,
                    static_cast<unsigned long long>(addr - base));
                return;
            }
        }
        std::snprintf(buffer, bufferSize, "0x%llX", static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(address)));
#else
        std::snprintf(buffer, bufferSize, "0x%llX", static_cast<unsigned long long>(
            reinterpret_cast<std::uintptr_t>(address)));
#endif
    }
}
