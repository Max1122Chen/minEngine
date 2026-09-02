#include "Platform/EditorCrashDiagnostics.h"

#include "Runtime/Core/Log/LogSystem.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#endif

namespace minEngine
{
#if defined(_WIN32)
    namespace
    {
        std::filesystem::path ResolveCrashLogPath()
        {
            wchar_t modulePath[MAX_PATH]{};
            const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                return std::filesystem::path("ed_crash.log");
            }

            std::filesystem::path path(modulePath);
            path.remove_filename();
            path /= "ed_crash.log";
            return path;
        }

        void WriteModuleName(std::ostream& out, const void* address)
        {
            HMODULE moduleHandle = nullptr;
            if (!GetModuleHandleExA(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    static_cast<LPCSTR>(address),
                    &moduleHandle)
                || moduleHandle == nullptr)
            {
                out << "unknown";
                return;
            }

            char modulePath[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                out << "unknown";
                return;
            }

            const auto base = reinterpret_cast<std::uintptr_t>(moduleHandle);
            const auto addr = reinterpret_cast<std::uintptr_t>(address);
            const std::filesystem::path path(modulePath);
            out << path.filename().string() << "+0x" << std::hex << std::uppercase << (addr - base) << std::dec;
        }

        void WriteStackTrace(std::ostream& out, CONTEXT* context)
        {
            void* frames[64]{};
            const USHORT frameCount = CaptureStackBackTrace(
                context != nullptr ? 1 : 0,
                64,
                frames,
                nullptr);

            out << "Stack (" << frameCount << " frames):\n";
            for (USHORT frameIndex = 0; frameIndex < frameCount; ++frameIndex)
            {
                out << "  #" << frameIndex << " 0x"
                    << std::hex << std::uppercase
                    << reinterpret_cast<std::uintptr_t>(frames[frameIndex])
                    << std::dec << " (";
                WriteModuleName(out, frames[frameIndex]);
                out << ")\n";
            }

            if (context != nullptr)
            {
                out << "FaultRIP=0x" << std::hex << std::uppercase << context->Rip << " (";
                WriteModuleName(out, reinterpret_cast<const void*>(context->Rip));
                out << ")\n";
                out << "Context RSP=0x" << context->Rsp << " RBP=0x" << context->Rbp << std::dec << '\n';
            }
        }

        LONG WINAPI EditorUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
        {
            if (exceptionPointers == nullptr || exceptionPointers->ExceptionRecord == nullptr)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            const DWORD code = exceptionPointers->ExceptionRecord->ExceptionCode;
            if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION
                && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            const auto logPath = ResolveCrashLogPath();
            std::ofstream log(logPath, std::ios::out | std::ios::app);
            if (!log.is_open())
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            log << "\n=== Editor crash " << __DATE__ << ' ' << __TIME__ << " ===\n";
            log << "ExceptionCode=0x" << std::hex << std::uppercase << code << std::dec << '\n';
            log << "ExceptionAddress=0x" << std::hex << std::uppercase
                << reinterpret_cast<std::uintptr_t>(exceptionPointers->ExceptionRecord->ExceptionAddress)
                << std::dec << '\n';

            if (code == EXCEPTION_ACCESS_VIOLATION
                && exceptionPointers->ExceptionRecord->NumberParameters >= 2)
            {
                const auto* params = exceptionPointers->ExceptionRecord->ExceptionInformation;
                log << "AV operation=" << params[0] << " address=0x" << std::hex << std::uppercase
                    << params[1] << std::dec << '\n';
            }

            WriteStackTrace(log, exceptionPointers->ContextRecord);
            log.flush();

            ME_CORE_ERROR(
                "Unhandled exception 0x{:08X} at 0x{:p}. See '{}'.",
                code,
                exceptionPointers->ExceptionRecord->ExceptionAddress,
                logPath.string());

            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
#endif

    void InstallEditorCrashDiagnostics()
    {
#if defined(_WIN32)
        SetUnhandledExceptionFilter(EditorUnhandledExceptionFilter);
        ME_CORE_INFO("Editor crash diagnostics: logging to ed_crash.log beside Editor.exe.");
#else
        ME_CORE_WARN("Editor crash diagnostics: Windows-only; not installed.");
#endif
    }
}
