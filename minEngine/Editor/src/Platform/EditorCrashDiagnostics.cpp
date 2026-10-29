#include "Platform/EditorCrashDiagnostics.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/ThreadIdentity.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <thread>
#include <vector>
#endif

namespace minEngine
{
#if defined(_WIN32)
    namespace
    {
        /**
         * A crash record is only symbolisable against the exact binaries that were loaded: an address
         * carries no identity of its own, so a reader needs the module it belongs to and proof that the
         * file on disk is that same build. Every identity field below exists for that proof.
         */
        struct ImageIdentity
        {
            std::uintptr_t LoadBase = 0;
            std::uintptr_t PreferredBase = 0;
            std::uint64_t ImageSize = 0;
            std::uint32_t TimeStamp = 0;
            bool Valid = false;
        };

        ImageIdentity ReadImageIdentity(HMODULE module)
        {
            ImageIdentity identity{};
            if (module == nullptr)
            {
                return identity;
            }

            const auto* base = reinterpret_cast<const std::uint8_t*>(module);
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
            {
                return identity;
            }

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
            {
                return identity;
            }

            identity.LoadBase = reinterpret_cast<std::uintptr_t>(module);
            identity.PreferredBase = static_cast<std::uintptr_t>(nt->OptionalHeader.ImageBase);
            identity.ImageSize = nt->OptionalHeader.SizeOfImage;
            identity.TimeStamp = nt->FileHeader.TimeDateStamp;
            identity.Valid = true;
            return identity;
        }

        std::string ModuleFileName(HMODULE module)
        {
            char path[MAX_PATH]{};
            const DWORD length = GetModuleFileNameA(module, path, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                return std::string();
            }
            return std::filesystem::path(path).filename().string();
        }

        /**
         * The process loads the debug-flavoured images (libminEngined.dll / libMaximumEditord.dll), so
         * looking names up by their release spelling answers "not loaded" and leaves the record with no
         * base for the module that faulted. Enumerate what the loader actually holds instead.
         */
        void WriteLoadedModules(std::ostream& out)
        {
            const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
            if (snapshot == INVALID_HANDLE_VALUE)
            {
                out << "LoadedModules: <snapshot unavailable, error " << GetLastError() << ">\n";
                return;
            }

            std::vector<std::string> lines;
            MODULEENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            if (Module32First(snapshot, &entry))
            {
                do
                {
                    const ImageIdentity identity =
                        ReadImageIdentity(reinterpret_cast<HMODULE>(entry.modBaseAddr));
                    if (!identity.Valid)
                    {
                        continue;
                    }

                    char line[512]{};
                    std::snprintf(
                        line,
                        sizeof(line),
                        "  %s base=0x%llX imagebase=0x%llX size=0x%llX timestamp=0x%08lX\n",
                        entry.szModule,
                        static_cast<unsigned long long>(identity.LoadBase),
                        static_cast<unsigned long long>(identity.PreferredBase),
                        static_cast<unsigned long long>(identity.ImageSize),
                        static_cast<unsigned long>(identity.TimeStamp));
                    lines.emplace_back(line);
                } while (Module32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);

            out << "LoadedModules (" << lines.size() << "):\n";
            for (const std::string& line : lines)
            {
                out << line;
            }
        }

        /**
         * Frames only become engine or driver frames once the reader knows which module each address
         * belongs to, so the record names the loaded image next to the offset into it.
         */
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

            const std::string name = ModuleFileName(moduleHandle);
            if (name.empty())
            {
                out << "unknown";
                return;
            }

            const auto base = reinterpret_cast<std::uintptr_t>(moduleHandle);
            const auto addr = reinterpret_cast<std::uintptr_t>(address);
            out << name << "+0x" << std::hex << std::uppercase << (addr - base) << std::dec;
        }

        void WriteAddress(std::ostream& out, std::uintptr_t address)
        {
            out << "0x" << std::hex << std::uppercase << address << std::dec << " (";
            WriteModuleName(out, reinterpret_cast<const void*>(address));
            out << ")";
        }

        /**
         * The thread's entry point is the closest thing to its creation site that survives inside the
         * process: the loader keeps it for the life of the thread and it resolves like any other frame.
         */
        std::uintptr_t QueryThreadStartAddress(HANDLE thread)
        {
            using NtQueryInformationThreadFn = LONG(WINAPI*)(HANDLE, int, void*, ULONG, ULONG*);
            const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
            if (ntdll == nullptr)
            {
                return 0;
            }

            const auto query = reinterpret_cast<NtQueryInformationThreadFn>(
                GetProcAddress(ntdll, "NtQueryInformationThread"));
            if (query == nullptr)
            {
                return 0;
            }

            // ThreadQuerySetWin32StartAddress: the address passed to CreateThread/RtlUserThreadStart.
            constexpr int kThreadQuerySetWin32StartAddress = 9;
            void* startAddress = nullptr;
            if (query(GetCurrentThread(), kThreadQuerySetWin32StartAddress, &startAddress, sizeof(startAddress), nullptr) != 0)
            {
                return 0;
            }
            return reinterpret_cast<std::uintptr_t>(startAddress);
        }

        std::string QueryThreadName(HANDLE thread)
        {
            using GetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
            const HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
            if (kernel32 == nullptr)
            {
                return std::string();
            }

            const auto getDescription = reinterpret_cast<GetThreadDescriptionFn>(
                GetProcAddress(kernel32, "GetThreadDescription"));
            if (getDescription == nullptr)
            {
                return std::string();
            }

            PWSTR description = nullptr;
            if (FAILED(getDescription(thread, &description)) || description == nullptr)
            {
                return std::string();
            }

            std::string name;
            for (const wchar_t* cursor = description; *cursor != L'\0'; ++cursor)
            {
                name.push_back(*cursor < 0x80 ? static_cast<char>(*cursor) : '?');
            }
            LocalFree(description);
            return name;
        }

        void WriteFaultThread(std::ostream& out, DWORD threadId)
        {
            const HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
            const std::uintptr_t startAddress =
                thread != nullptr ? QueryThreadStartAddress(thread) : std::uintptr_t{0};
            const std::string name = thread != nullptr ? QueryThreadName(thread) : std::string();
            if (thread != nullptr)
            {
                CloseHandle(thread);
            }

            const ThreadIdentity identity = CaptureCurrentThreadIdentity();
            out << "FaultThread: id=" << std::dec << threadId
                << " logid=" << identity.LogThreadId
                << " role=" << identity.RoleName
                << " name=\"" << (name.empty() ? "unnamed" : name) << "\" start=";
            if (startAddress == 0)
            {
                out << "<unavailable>";
            }
            else
            {
                WriteAddress(out, startAddress);
            }
            out << '\n';
        }

        void WriteContextRegisters(std::ostream& out, CONTEXT* context)
        {
            if (context == nullptr)
            {
                return;
            }

            out << "FaultRIP=";
            WriteAddress(out, context->Rip);
            out << '\n';
            out << "RAX=0x" << std::hex << std::uppercase << context->Rax
                << " RCX=0x" << context->Rcx
                << " RDX=0x" << context->Rdx
                << " RBX=0x" << context->Rbx << std::dec << '\n';
            out << "RSP=0x" << std::hex << std::uppercase << context->Rsp
                << " RBP=0x" << context->Rbp
                << " RSI=0x" << context->Rsi
                << " RDI=0x" << context->Rdi << std::dec << '\n';
            out << "R8=0x" << std::hex << std::uppercase << context->R8
                << " R9=0x" << context->R9
                << " R10=0x" << context->R10
                << " R11=0x" << context->R11 << std::dec << '\n';
        }

        bool IsReadableAddress(const void* address, size_t byteCount)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (::VirtualQuery(address, &info, sizeof(info)) == 0)
            {
                return false;
            }
            if (info.State != MEM_COMMIT)
            {
                return false;
            }
            const DWORD protect = info.Protect & 0xFF;
            return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY
                || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE
                || protect == PAGE_EXECUTE_WRITECOPY;
        }

        // CaptureStackBackTrace walks from *this* filter frame and is useless when RIP is
        // already null/garbage. Dump the faulting CONTEXT registers and the words on the
        // faulting stack — those return addresses survive even when RIP itself does not.
        void WriteFaultingStackWords(std::ostream& out, CONTEXT* context)
        {
            if (context == nullptr)
            {
                return;
            }

            constexpr int kWordCount = 48;
            out << "FaultStackWords (" << kWordCount << " from RSP):\n";
            for (int wordIndex = 0; wordIndex < kWordCount; ++wordIndex)
            {
                const auto* slot = reinterpret_cast<const std::uintptr_t*>(context->Rsp) + wordIndex;
                if (!IsReadableAddress(slot, sizeof(std::uintptr_t)))
                {
                    out << "  [" << wordIndex << "] <unreadable>\n";
                    break;
                }

                const std::uintptr_t word = *slot;
                out << "  [" << wordIndex << "] ";
                WriteAddress(out, word);
                out << '\n';
            }
        }

        void WriteStackTrace(std::ostream& out, CONTEXT* context)
        {
            WriteContextRegisters(out, context);
            WriteFaultingStackWords(out, context);

            void* frames[64]{};
            const USHORT frameCount = CaptureStackBackTrace(0, 64, frames, nullptr);

            out << "FilterStack (" << frameCount << " frames, CaptureStackBackTrace):\n";
            for (USHORT frameIndex = 0; frameIndex < frameCount; ++frameIndex)
            {
                out << "  #" << frameIndex << " ";
                WriteAddress(out, reinterpret_cast<std::uintptr_t>(frames[frameIndex]));
                out << '\n';
            }
        }

        /**
         * A minidump carries the full thread table, stacks and the loaded module list, which is the only
         * artefact that survives a fault raised inside a driver's own worker thread: those frames exist
         * nowhere in the process until the debugger walks the dump.
         *
         * dbghelp.dll is loaded at crash time rather than linked: it ships with Windows, so the editor
         * gains no new dependency, nothing to redistribute, and a missing dbghelp costs a log line
         * instead of the whole record.
         */
        struct MiniDumpExceptionInformation
        {
            DWORD ThreadId;
            EXCEPTION_POINTERS* ExceptionPointers;
            BOOL ClientPointers;
        };

        using MiniDumpWriteDumpFn = BOOL(WINAPI*)(
            HANDLE, DWORD, HANDLE, int, MiniDumpExceptionInformation*, void*, void*);

        /**
         * Returns the path that was written, or why nothing was: a dump that silently does not exist
         * is worse than no dump at all, because the record then claims evidence it does not have.
         */
        std::string WriteMiniDump(
            const std::filesystem::path& path,
            EXCEPTION_POINTERS* exceptionPointers,
            DWORD threadId)
        {
            const HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
            if (dbghelp == nullptr)
            {
                return "<not written: dbghelp.dll unavailable, error "
                    + std::to_string(GetLastError()) + ">";
            }

            const auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(
                GetProcAddress(dbghelp, "MiniDumpWriteDump"));
            if (writeDump == nullptr)
            {
                FreeLibrary(dbghelp);
                return "<not written: MiniDumpWriteDump not exported>";
            }

            const HANDLE file = CreateFileW(
                path.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                const DWORD error = GetLastError();
                FreeLibrary(dbghelp);
                return "<not written: cannot create the file, error " + std::to_string(error) + ">";
            }

            // MiniDumpNormal plus thread info: every thread's stack, no heap, so the dump stays small
            // enough to attach to a report while still holding the frames this record cannot name.
            constexpr int kMiniDumpNormal = 0x00000000;
            constexpr int kMiniDumpWithThreadInfo = 0x00001000;

            MiniDumpExceptionInformation info{};
            info.ThreadId = threadId;
            info.ExceptionPointers = exceptionPointers;
            info.ClientPointers = FALSE;

            // Some dbghelp builds refuse the exception record of a process dumping itself, so the
            // attempt degrades instead of losing the stacks: the thread table is the part that
            // matters for a fault raised inside a driver worker thread.
            BOOL written = writeDump(
                GetCurrentProcess(),
                GetCurrentProcessId(),
                file,
                kMiniDumpNormal | kMiniDumpWithThreadInfo,
                &info,
                nullptr,
                nullptr);
            DWORD error = written != FALSE ? 0 : GetLastError();
            if (written == FALSE)
            {
                info.ExceptionPointers = nullptr;
                written = writeDump(
                    GetCurrentProcess(),
                    GetCurrentProcessId(),
                    file,
                    kMiniDumpNormal | kMiniDumpWithThreadInfo,
                    &info,
                    nullptr,
                    nullptr);
                error = written != FALSE ? 0 : GetLastError();
            }
            if (written == FALSE)
            {
                written = writeDump(
                    GetCurrentProcess(),
                    GetCurrentProcessId(),
                    file,
                    kMiniDumpNormal,
                    nullptr,
                    nullptr,
                    nullptr);
                error = written != FALSE ? 0 : GetLastError();
            }
            CloseHandle(file);
            FreeLibrary(dbghelp);
            if (written == FALSE)
            {
                return "<not written: MiniDumpWriteDump failed, error "
                    + std::to_string(error) + ">";
            }
            return path.string();
        }

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

        std::string TimestampForFile(const char* timestamp)
        {
            std::string name(timestamp);
            for (char& ch : name)
            {
                if (ch == ' ' || ch == ':' || ch == '.')
                {
                    ch = '-';
                }
            }
            return name;
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

            SYSTEMTIME crashedAt{};
            GetLocalTime(&crashedAt);
            char timestamp[64]{};
            std::snprintf(
                timestamp,
                sizeof(timestamp),
                "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                crashedAt.wYear,
                crashedAt.wMonth,
                crashedAt.wDay,
                crashedAt.wHour,
                crashedAt.wMinute,
                crashedAt.wSecond,
                crashedAt.wMilliseconds);

            log << "\n=== Editor crash " << timestamp << " ===\n";
            WriteLoadedModules(log);

            const DWORD faultingThreadId = GetCurrentThreadId();
            WriteFaultThread(log, faultingThreadId);
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

            std::filesystem::path dumpPath = logPath;
            dumpPath.remove_filename();
            dumpPath /= "ed_crash_" + TimestampForFile(timestamp) + ".dmp";
            log << "MiniDump: " << WriteMiniDump(dumpPath, exceptionPointers, faultingThreadId) << '\n';
            log.flush();

            ME_LOG(LogEditor, Error,
                "Unhandled exception 0x{:08X} at {:p} on thread {}. See '{}'.",
                code,
                exceptionPointers->ExceptionRecord->ExceptionAddress,
                faultingThreadId,
                logPath.string());

            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
#endif

    void InstallEditorCrashDiagnostics()
    {
#if defined(_WIN32)
        SetUnhandledExceptionFilter(EditorUnhandledExceptionFilter);
        ME_LOG(LogEditor, Info, "Editor crash diagnostics: logging to ed_crash.log beside MaximumEditor.exe.");
#else
        ME_LOG(LogEditor, Warn, "Editor crash diagnostics: Windows-only; not installed.");
#endif
    }
}
