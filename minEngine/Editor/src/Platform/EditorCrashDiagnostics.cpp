#include "Platform/EditorCrashDiagnostics.h"

#include "Runtime/Core/Threading/ThreadIdentity.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#include <malloc.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#endif

/**
 * What the fault path is allowed to do, and why it is this small.
 *
 * The filter runs on a thread that has already failed, often while that thread holds a lock. A
 * record path that allocates, formats through the C++ library, asks the log system for its mutex or
 * walks the loader list under the loader lock turns "the process crashes" into "the process hangs,
 * and the one artefact that would explain it is never written". So the fault path does exactly one
 * thing: it copies the fault to static storage with plain stores and hands it to a thread that is
 * waiting with a healthy stack. Everything else - formatting, the module list, the dump - happens
 * there.
 *
 * The reserved set is small and fixed:
 *   - one static record buffer, formatted by hand (no std::string, no std::ostream, no vector);
 *   - one static copy of the exception record and the faulting CONTEXT;
 *   - the loaded-module table, read straight out of the process environment block;
 *   - one waiting thread with a stack of its own;
 *   - the record file, written with WriteFile on a handle opened at fault time.
 *
 * Nothing on that list takes a lock the faulting thread could be holding, and nothing on it grows
 * the heap.
 */
namespace minEngine
{
#if defined(_WIN32)
    namespace
    {
        constexpr unsigned kRecordCapacity = 128 * 1024;
        constexpr unsigned kMaxRecordedModules = 256;
        constexpr unsigned kMaxModuleNameChars = 48;
        constexpr unsigned kFaultStackWordCount = 48;
        constexpr unsigned kFilterFrameCount = 32;
        constexpr unsigned kPathChars = MAX_PATH * 2;
        constexpr unsigned kThreadNameChars = 96;

        // The faulting thread gives up rather than waiting for a worker that may itself be blocked:
        // a record that is already on disk is worth more than a process that never ends.
        constexpr DWORD kRecordWaitMs = 20000;
        constexpr DWORD kDumpWaitMs = 30000;

        // A thread that overflowed its stack needs a page back to even reach the hand-off.
        constexpr ULONG kFaultStackGuaranteeBytes = 64 * 1024;
        constexpr DWORD kRecordThreadStackBytes = 1024 * 1024;

        /** The loader's own view of a module. Offsets are the x64 loader list layout. */
        struct LoaderLinks
        {
            void* Flink;
            void* Blink;
        };

        struct LoaderName
        {
            unsigned short Length;
            unsigned short MaximumLength;
            const wchar_t* Buffer;
        };

        struct LoaderEntry
        {
            LoaderLinks InLoadOrderLinks;
            LoaderLinks InMemoryOrderLinks;
            LoaderLinks InInitializationOrderLinks;
            void* DllBase;
            void* EntryPoint;
            unsigned long SizeOfImage;
            LoaderName FullDllName;
            LoaderName BaseDllName;
        };

        struct PebLoaderData
        {
            unsigned long Length;
            unsigned char Initialized;
            unsigned char Padding[3];
            void* SsHandle;
            LoaderLinks InLoadOrderModuleList;
            LoaderLinks InMemoryOrderModuleList;
            LoaderLinks InInitializationOrderModuleList;
        };

        struct ProcessEnvironmentBlock
        {
            unsigned char Reserved[0x18];
            void* LoaderData;
        };

        static_assert(offsetof(ProcessEnvironmentBlock, LoaderData) == 0x18, "PEB.Ldr sits at 0x18");
        static_assert(offsetof(PebLoaderData, InMemoryOrderModuleList) == 0x20, "loader list layout");
        static_assert(offsetof(LoaderEntry, DllBase) == 0x30, "loader entry layout");
        static_assert(offsetof(LoaderEntry, SizeOfImage) == 0x40, "loader entry layout");
        static_assert(offsetof(LoaderEntry, BaseDllName) == 0x58, "loader entry layout");

        struct ModuleSpan
        {
            std::uintptr_t LoadBase = 0;
            std::uintptr_t PreferredBase = 0;
            unsigned long long ImageSize = 0;
            unsigned long TimeStamp = 0;
            char Name[kMaxModuleNameChars]{};
        };

        /**
         * Everything the record needs, in storage that outlives the fault: a thread that faulted
         * cannot be assumed to return, so the record must not live in its frame.
         */
        struct CrashState
        {
            wchar_t DirectoryW[kPathChars]{};
            wchar_t RecordPathW[kPathChars]{};
            wchar_t DumpPathW[kPathChars]{};
            char RecordPathA[kPathChars]{};

            HANDLE FaultEvent = nullptr;
            HANDLE RecordWrittenEvent = nullptr;
            HANDLE RecordDoneEvent = nullptr;
            HANDLE RecordThread = nullptr;
            DWORD RecordThreadId = 0;
            bool Installed = false;

            // Written by the faulting thread before it signals, read by the record thread after.
            EXCEPTION_RECORD Exception{};
            CONTEXT Context{};
            bool HasContext = false;
            DWORD FaultThreadId = 0;
            std::uint32_t FaultLogThreadId = 0;
            const char* FaultRoleName = "none";
            void* FilterFrames[kFilterFrameCount]{};
            unsigned FilterFrameCount = 0;

            // Written by the record thread.
            ModuleSpan Modules[kMaxRecordedModules]{};
            unsigned ModuleCount = 0;
            char Record[kRecordCapacity]{};
            bool RecordFileWritten = false;

            // The once guard: one record path per process, never nested.
            volatile LONG FaultPathLatch = 0;
            volatile LONG Admitted = 0;
            volatile LONG Rejected = 0;
            volatile LONG SelfCheckReentry = 0;
        };

        CrashState& State()
        {
            // Never destroyed: the record thread may still be reading it while the process exits.
            static CrashState state;
            return state;
        }

        // ---- text built by hand, into the reserved buffer -----------------------

        struct TextBuffer
        {
            char* Data = nullptr;
            unsigned Capacity = 0;
            unsigned Size = 0;
            bool Truncated = false;
        };

        void AppendChar(TextBuffer& buffer, char value)
        {
            if (buffer.Data == nullptr || buffer.Size + 1 >= buffer.Capacity)
            {
                buffer.Truncated = true;
                return;
            }
            buffer.Data[buffer.Size++] = value;
            buffer.Data[buffer.Size] = '\0';
        }

        void AppendText(TextBuffer& buffer, const char* text)
        {
            if (text == nullptr)
            {
                return;
            }
            for (const char* cursor = text; *cursor != '\0'; ++cursor)
            {
                AppendChar(buffer, *cursor);
            }
        }

        void AppendHex(TextBuffer& buffer, unsigned long long value)
        {
            char digits[16]{};
            int used = 0;
            do
            {
                const int digit = static_cast<int>(value & 0xFULL);
                digits[used++] = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
                value >>= 4;
            } while (value != 0);

            while (used > 0)
            {
                AppendChar(buffer, digits[--used]);
            }
        }

        void AppendDecimal(TextBuffer& buffer, unsigned long long value, unsigned width)
        {
            char digits[24]{};
            int used = 0;
            do
            {
                digits[used++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0);

            while (static_cast<unsigned>(used) < width)
            {
                digits[used++] = '0';
            }
            while (used > 0)
            {
                AppendChar(buffer, digits[--used]);
            }
        }

        void AppendWideText(char* destination, unsigned capacity, const wchar_t* text, unsigned length)
        {
            if (destination == nullptr || capacity == 0 || text == nullptr)
            {
                return;
            }
            unsigned written = 0;
            for (unsigned index = 0; index < length && written + 1 < capacity; ++index)
            {
                const wchar_t wide = text[index];
                destination[written++] = wide < 0x80 ? static_cast<char>(wide) : '?';
            }
            destination[written] = '\0';
        }

        /** Reads are the only thing the record may still do to memory it does not own. */
        bool IsReadableAddress(const void* address, size_t byteCount)
        {
            if (address == nullptr)
            {
                return false;
            }

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

        // ---- the loaded-module table -------------------------------------------

        bool ReadImageIdentity(const void* base, ModuleSpan& span)
        {
            if (!IsReadableAddress(base, sizeof(IMAGE_DOS_HEADER)))
            {
                return false;
            }

            const auto* dos = static_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            {
                return false;
            }

            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                static_cast<const std::uint8_t*>(base) + dos->e_lfanew);
            if (!IsReadableAddress(nt, sizeof(IMAGE_NT_HEADERS64)) || nt->Signature != IMAGE_NT_SIGNATURE)
            {
                return false;
            }

            span.Name[0] = '\0';
            span.LoadBase = reinterpret_cast<std::uintptr_t>(base);
            span.PreferredBase = static_cast<std::uintptr_t>(nt->OptionalHeader.ImageBase);
            span.ImageSize = nt->OptionalHeader.SizeOfImage;
            span.TimeStamp = nt->FileHeader.TimeDateStamp;
            return true;
        }

        /**
         * Enumerated from the loader's own list instead of a toolhelp snapshot: the snapshot
         * allocates and synchronises with the loader lock, and a fault raised while that lock is
         * held would then cost the whole record.
         */
        void CaptureLoadedModules()
        {
            CrashState& state = State();
            state.ModuleCount = 0;

            const auto* peb = reinterpret_cast<const ProcessEnvironmentBlock*>(
                ::NtCurrentTeb()->ProcessEnvironmentBlock);
            if (!IsReadableAddress(peb, sizeof(*peb)))
            {
                return;
            }

            const auto* loader = reinterpret_cast<const PebLoaderData*>(peb->LoaderData);
            if (!IsReadableAddress(loader, sizeof(*loader)))
            {
                return;
            }

            const LoaderLinks* head = &loader->InMemoryOrderModuleList;
            const void* cursor = head->Flink;
            while (cursor != nullptr && cursor != head && state.ModuleCount < kMaxRecordedModules)
            {
                const auto* entry = reinterpret_cast<const LoaderEntry*>(
                    static_cast<const std::uint8_t*>(cursor) - offsetof(LoaderEntry, InMemoryOrderLinks));
                if (!IsReadableAddress(entry, sizeof(*entry)))
                {
                    break;
                }

                ModuleSpan& span = state.Modules[state.ModuleCount];
                if (ReadImageIdentity(entry->DllBase, span))
                {
                    const unsigned length = entry->BaseDllName.Length / sizeof(wchar_t);
                    if (IsReadableAddress(entry->BaseDllName.Buffer, entry->BaseDllName.Length))
                    {
                        AppendWideText(span.Name, kMaxModuleNameChars, entry->BaseDllName.Buffer, length);
                    }
                    else
                    {
                        const char* placeholder = "<unnamed>";
                        for (unsigned index = 0; placeholder[index] != '\0' && index + 1 < kMaxModuleNameChars;
                             ++index)
                        {
                            span.Name[index] = placeholder[index];
                        }
                    }
                    ++state.ModuleCount;
                }

                cursor = entry->InMemoryOrderLinks.Flink;
            }
        }

        const ModuleSpan* FindModule(std::uintptr_t address)
        {
            const CrashState& state = State();
            for (unsigned index = 0; index < state.ModuleCount; ++index)
            {
                const ModuleSpan& span = state.Modules[index];
                if (span.LoadBase != 0 && address >= span.LoadBase
                    && address < span.LoadBase + span.ImageSize)
                {
                    return &span;
                }
            }
            return nullptr;
        }

        void AppendAddress(TextBuffer& buffer, std::uintptr_t address)
        {
            AppendText(buffer, "0x");
            AppendHex(buffer, address);
            AppendText(buffer, " (");
            const ModuleSpan* owner = FindModule(address);
            if (owner == nullptr)
            {
                AppendText(buffer, "unknown");
            }
            else
            {
                AppendText(buffer, owner->Name);
                AppendText(buffer, "+0x");
                AppendHex(buffer, address - owner->LoadBase);
            }
            AppendText(buffer, ")");
        }

        // ---- what only the faulting thread could answer ------------------------

        std::uintptr_t QueryThreadStartAddress(HANDLE thread)
        {
            using NtQueryInformationThreadFn = LONG(WINAPI*)(HANDLE, int, void*, ULONG, ULONG*);
            const HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");
            if (ntdll == nullptr)
            {
                return 0;
            }

            const auto query = reinterpret_cast<NtQueryInformationThreadFn>(
                ::GetProcAddress(ntdll, "NtQueryInformationThread"));
            if (query == nullptr)
            {
                return 0;
            }

            // ThreadQuerySetWin32StartAddress: the address passed to CreateThread/RtlUserThreadStart,
            // which is the closest thing to the thread's creation site that survives in-process.
            constexpr int kThreadQuerySetWin32StartAddress = 9;
            void* startAddress = nullptr;
            if (query(thread, kThreadQuerySetWin32StartAddress, &startAddress, sizeof(startAddress), nullptr) != 0)
            {
                return 0;
            }
            return reinterpret_cast<std::uintptr_t>(startAddress);
        }

        void QueryThreadName(HANDLE thread, char* destination, unsigned capacity)
        {
            if (destination == nullptr || capacity == 0)
            {
                return;
            }
            destination[0] = '\0';

            using GetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
            const HMODULE kernel32 = ::GetModuleHandleA("kernel32.dll");
            if (kernel32 == nullptr)
            {
                return;
            }

            const auto getDescription = reinterpret_cast<GetThreadDescriptionFn>(
                ::GetProcAddress(kernel32, "GetThreadDescription"));
            if (getDescription == nullptr)
            {
                return;
            }

            PWSTR description = nullptr;
            if (FAILED(getDescription(thread, &description)) || description == nullptr)
            {
                return;
            }

            unsigned length = 0;
            while (description[length] != L'\0')
            {
                ++length;
            }
            AppendWideText(destination, capacity, description, length);
            ::LocalFree(description);
        }

        // ---- the record --------------------------------------------------------

        void AppendTimestamp(TextBuffer& buffer)
        {
            SYSTEMTIME crashedAt{};
            ::GetLocalTime(&crashedAt);
            AppendDecimal(buffer, crashedAt.wYear, 4);
            AppendChar(buffer, '-');
            AppendDecimal(buffer, crashedAt.wMonth, 2);
            AppendChar(buffer, '-');
            AppendDecimal(buffer, crashedAt.wDay, 2);
            AppendChar(buffer, ' ');
            AppendDecimal(buffer, crashedAt.wHour, 2);
            AppendChar(buffer, ':');
            AppendDecimal(buffer, crashedAt.wMinute, 2);
            AppendChar(buffer, ':');
            AppendDecimal(buffer, crashedAt.wSecond, 2);
            AppendChar(buffer, '.');
            AppendDecimal(buffer, crashedAt.wMilliseconds, 3);
        }

        void AppendLoadedModules(TextBuffer& buffer)
        {
            const CrashState& state = State();
            AppendText(buffer, "LoadedModules (");
            AppendDecimal(buffer, state.ModuleCount, 1);
            AppendText(buffer, "):\n");

            if (state.ModuleCount == 0)
            {
                AppendText(buffer, "  <loader list unreadable>\n");
                return;
            }

            for (unsigned index = 0; index < state.ModuleCount; ++index)
            {
                const ModuleSpan& span = state.Modules[index];
                AppendText(buffer, "  ");
                AppendText(buffer, span.Name);
                AppendText(buffer, " base=0x");
                AppendHex(buffer, span.LoadBase);
                AppendText(buffer, " imagebase=0x");
                AppendHex(buffer, span.PreferredBase);
                AppendText(buffer, " size=0x");
                AppendHex(buffer, span.ImageSize);
                AppendText(buffer, " timestamp=0x");
                AppendHex(buffer, span.TimeStamp);
                AppendChar(buffer, '\n');
            }
        }

        void AppendFaultThread(TextBuffer& buffer)
        {
            const CrashState& state = State();

            std::uintptr_t startAddress = 0;
            char name[kThreadNameChars]{};
            const HANDLE thread = ::OpenThread(THREAD_QUERY_INFORMATION, FALSE, state.FaultThreadId);
            if (thread != nullptr)
            {
                startAddress = QueryThreadStartAddress(thread);
                QueryThreadName(thread, name, sizeof(name));
                ::CloseHandle(thread);
            }

            AppendText(buffer, "FaultThread: id=");
            AppendDecimal(buffer, state.FaultThreadId, 1);
            AppendText(buffer, " logid=");
            AppendDecimal(buffer, state.FaultLogThreadId, 1);
            AppendText(buffer, " role=");
            AppendText(buffer, state.FaultRoleName);
            AppendText(buffer, " name=\"");
            AppendText(buffer, name[0] != '\0' ? name : "unnamed");
            AppendText(buffer, "\" start=");
            if (startAddress == 0)
            {
                AppendText(buffer, "<unavailable>");
            }
            else
            {
                AppendAddress(buffer, startAddress);
            }
            AppendChar(buffer, '\n');
        }

        void AppendContextRegisters(TextBuffer& buffer)
        {
            const CrashState& state = State();
            if (!state.HasContext)
            {
                return;
            }

            const CONTEXT& context = state.Context;
            AppendText(buffer, "FaultRIP=");
            AppendAddress(buffer, context.Rip);
            AppendChar(buffer, '\n');

            AppendText(buffer, "RAX=0x");
            AppendHex(buffer, context.Rax);
            AppendText(buffer, " RCX=0x");
            AppendHex(buffer, context.Rcx);
            AppendText(buffer, " RDX=0x");
            AppendHex(buffer, context.Rdx);
            AppendText(buffer, " RBX=0x");
            AppendHex(buffer, context.Rbx);
            AppendChar(buffer, '\n');

            AppendText(buffer, "RSP=0x");
            AppendHex(buffer, context.Rsp);
            AppendText(buffer, " RBP=0x");
            AppendHex(buffer, context.Rbp);
            AppendText(buffer, " RSI=0x");
            AppendHex(buffer, context.Rsi);
            AppendText(buffer, " RDI=0x");
            AppendHex(buffer, context.Rdi);
            AppendChar(buffer, '\n');

            AppendText(buffer, "R8=0x");
            AppendHex(buffer, context.R8);
            AppendText(buffer, " R9=0x");
            AppendHex(buffer, context.R9);
            AppendText(buffer, " R10=0x");
            AppendHex(buffer, context.R10);
            AppendText(buffer, " R11=0x");
            AppendHex(buffer, context.R11);
            AppendChar(buffer, '\n');
        }

        /**
         * CaptureStackBackTrace walks from the filter frame, so it says little about a fault whose
         * RIP is already garbage. The words on the faulting stack are the part that survives: those
         * return addresses are still there even when the faulting frame is not.
         */
        void AppendFaultingStackWords(TextBuffer& buffer)
        {
            const CrashState& state = State();
            if (!state.HasContext)
            {
                return;
            }

            AppendText(buffer, "FaultStackWords (");
            AppendDecimal(buffer, kFaultStackWordCount, 1);
            AppendText(buffer, " from RSP):\n");

            for (unsigned wordIndex = 0; wordIndex < kFaultStackWordCount; ++wordIndex)
            {
                const auto* slot = reinterpret_cast<const std::uintptr_t*>(state.Context.Rsp) + wordIndex;
                if (!IsReadableAddress(slot, sizeof(std::uintptr_t)))
                {
                    AppendText(buffer, "  [");
                    AppendDecimal(buffer, wordIndex, 1);
                    AppendText(buffer, "] <unreadable>\n");
                    break;
                }

                AppendText(buffer, "  [");
                AppendDecimal(buffer, wordIndex, 1);
                AppendText(buffer, "] ");
                AppendAddress(buffer, *slot);
                AppendChar(buffer, '\n');
            }
        }

        void AppendFilterStack(TextBuffer& buffer)
        {
            const CrashState& state = State();
            AppendText(buffer, "FilterStack (");
            AppendDecimal(buffer, state.FilterFrameCount, 1);
            AppendText(buffer, " frames, CaptureStackBackTrace):\n");
            for (unsigned frameIndex = 0; frameIndex < state.FilterFrameCount; ++frameIndex)
            {
                AppendText(buffer, "  #");
                AppendDecimal(buffer, frameIndex, 1);
                AppendChar(buffer, ' ');
                AppendAddress(buffer, reinterpret_cast<std::uintptr_t>(state.FilterFrames[frameIndex]));
                AppendChar(buffer, '\n');
            }
        }

        void AppendExceptionFacts(TextBuffer& buffer)
        {
            const CrashState& state = State();
            const EXCEPTION_RECORD& record = state.Exception;

            AppendText(buffer, "ExceptionCode=0x");
            AppendHex(buffer, record.ExceptionCode);
            AppendChar(buffer, '\n');
            AppendText(buffer, "ExceptionAddress=0x");
            AppendHex(buffer, reinterpret_cast<std::uintptr_t>(record.ExceptionAddress));
            AppendChar(buffer, '\n');

            if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
            {
                AppendText(buffer, "AV operation=");
                AppendDecimal(buffer, record.ExceptionInformation[0], 1);
                AppendText(buffer, " address=0x");
                AppendHex(buffer, record.ExceptionInformation[1]);
                AppendChar(buffer, '\n');
            }
        }

        // ---- the dump, off the faulting stack ----------------------------------

        struct MiniDumpExceptionInformation
        {
            DWORD ThreadId;
            EXCEPTION_POINTERS* ExceptionPointers;
            BOOL ClientPointers;
        };

        using MiniDumpWriteDumpFn = BOOL(WINAPI*)(
            HANDLE, DWORD, HANDLE, int, MiniDumpExceptionInformation*, void*, void*);

        /**
         * A minidump carries the thread table, the stacks and the module list, which is the only
         * artefact that survives a fault raised inside a driver's own worker thread.
         *
         * dbghelp.dll is loaded here rather than linked: it ships with Windows, so the editor gains
         * no dependency and a missing dbghelp costs a line instead of the record. Loading it takes
         * the loader lock - which is exactly why this runs after the record is already on disk and
         * on a thread whose stack is intact.
         */
        void AppendMiniDumpLine(TextBuffer& status, const wchar_t* dumpPath)
        {
            const HMODULE dbghelp = ::LoadLibraryW(L"dbghelp.dll");
            if (dbghelp == nullptr)
            {
                AppendText(status, "<not written: dbghelp.dll unavailable, error ");
                AppendDecimal(status, ::GetLastError(), 1);
                AppendText(status, ">");
                return;
            }

            const auto writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(
                ::GetProcAddress(dbghelp, "MiniDumpWriteDump"));
            if (writeDump == nullptr)
            {
                ::FreeLibrary(dbghelp);
                AppendText(status, "<not written: MiniDumpWriteDump not exported>");
                return;
            }

            const HANDLE file = ::CreateFileW(
                dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                const DWORD error = ::GetLastError();
                ::FreeLibrary(dbghelp);
                AppendText(status, "<not written: cannot create the file, error ");
                AppendDecimal(status, error, 1);
                AppendText(status, ">");
                return;
            }

            // MiniDumpNormal plus thread info: every thread's stack, no heap, so the dump stays
            // small enough to attach while still holding the frames the record cannot name.
            constexpr int kMiniDumpNormal = 0x00000000;
            constexpr int kMiniDumpWithThreadInfo = 0x00001000;

            CrashState& state = State();
            EXCEPTION_POINTERS pointers{&state.Exception, state.HasContext ? &state.Context : nullptr};
            MiniDumpExceptionInformation info{};
            info.ThreadId = state.FaultThreadId;
            info.ExceptionPointers = state.HasContext ? &pointers : nullptr;
            info.ClientPointers = FALSE;

            // Some dbghelp builds refuse the exception record of a process dumping itself, so the
            // attempt degrades instead of losing the stacks.
            BOOL written = writeDump(
                ::GetCurrentProcess(),
                ::GetCurrentProcessId(),
                file,
                kMiniDumpNormal | kMiniDumpWithThreadInfo,
                &info,
                nullptr,
                nullptr);
            DWORD error = written != FALSE ? 0 : ::GetLastError();
            if (written == FALSE)
            {
                info.ExceptionPointers = nullptr;
                written = writeDump(
                    ::GetCurrentProcess(),
                    ::GetCurrentProcessId(),
                    file,
                    kMiniDumpNormal | kMiniDumpWithThreadInfo,
                    &info,
                    nullptr,
                    nullptr);
                error = written != FALSE ? 0 : ::GetLastError();
            }
            if (written == FALSE)
            {
                written = writeDump(
                    ::GetCurrentProcess(),
                    ::GetCurrentProcessId(),
                    file,
                    kMiniDumpNormal,
                    nullptr,
                    nullptr,
                    nullptr);
                error = written != FALSE ? 0 : ::GetLastError();
            }

            ::CloseHandle(file);
            ::FreeLibrary(dbghelp);

            if (written == FALSE)
            {
                AppendText(status, "<not written: MiniDumpWriteDump failed, error ");
                AppendDecimal(status, error, 1);
                AppendText(status, ">");
                return;
            }

            // The path as ANSI, from the wide path the fault path prepared.
            for (unsigned index = 0; dumpPath[index] != L'\0' && index < kPathChars; ++index)
            {
                AppendChar(status, dumpPath[index] < 0x80 ? static_cast<char>(dumpPath[index]) : '?');
            }
        }

        // ---- writing the record ------------------------------------------------

        bool AppendToRecordFile(const char* data, unsigned size)
        {
            const CrashState& state = State();
            const HANDLE file = ::CreateFileW(
                state.RecordPathW,
                FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            DWORD written = 0;
            const BOOL ok = ::WriteFile(file, data, size, &written, nullptr);
            ::CloseHandle(file);
            return ok != FALSE && written == size;
        }

        void BuildDumpPath()
        {
            CrashState& state = State();

            unsigned index = 0;
            for (; state.DirectoryW[index] != L'\0' && index < kPathChars - 1; ++index)
            {
                state.DumpPathW[index] = state.DirectoryW[index];
            }

            const wchar_t* stem = L"ed_crash_";
            for (unsigned stemIndex = 0; stem[stemIndex] != L'\0' && index < kPathChars - 1; ++stemIndex)
            {
                state.DumpPathW[index++] = stem[stemIndex];
            }

            // The file name carries the same wall clock as the record, so a dump is found by
            // reading the record rather than by comparing file times.
            SYSTEMTIME crashedAt{};
            ::GetLocalTime(&crashedAt);
            const WORD parts[7] = {crashedAt.wYear, crashedAt.wMonth, crashedAt.wDay, crashedAt.wHour,
                crashedAt.wMinute, crashedAt.wSecond, crashedAt.wMilliseconds};
            const unsigned widths[7] = {4, 2, 2, 2, 2, 2, 3};
            for (int part = 0; part < 7; ++part)
            {
                if (part != 0 && index < kPathChars - 1)
                {
                    state.DumpPathW[index++] = L'-';
                }
                wchar_t digits[8]{};
                int used = 0;
                WORD value = parts[part];
                do
                {
                    digits[used++] = static_cast<wchar_t>(L'0' + value % 10);
                    value = static_cast<WORD>(value / 10);
                } while (value != 0);
                while (static_cast<unsigned>(used) < widths[part])
                {
                    digits[used++] = L'0';
                }
                while (used > 0 && index < kPathChars - 1)
                {
                    state.DumpPathW[index++] = digits[--used];
                }
            }

            const wchar_t* suffix = L".dmp";
            for (unsigned suffixIndex = 0; suffix[suffixIndex] != L'\0' && index < kPathChars - 1; ++suffixIndex)
            {
                state.DumpPathW[index++] = suffix[suffixIndex];
            }
            state.DumpPathW[index] = L'\0';
        }

        /** Raises a second fault from inside the record path: the once guard has to turn it away. */
        LONG WINAPI EditorUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers);

        void ProvokeReentryForSelfCheck()
        {
            if (::InterlockedExchange(&State().SelfCheckReentry, 0) == 0)
            {
                return;
            }

            EXCEPTION_RECORD record{};
            record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
            record.ExceptionAddress = reinterpret_cast<void*>(&ProvokeReentryForSelfCheck);
            record.NumberParameters = 2;
            record.ExceptionInformation[0] = 1;
            record.ExceptionInformation[1] = 0x10;

            CONTEXT context;
            ::RtlCaptureContext(&context);
            EXCEPTION_POINTERS pointers{&record, &context};

            // The latch is already taken by the record that is being written right now, so this
            // call must return without touching the record.
            EditorUnhandledExceptionFilter(&pointers);
        }

        void WriteRecordOnHealthyStack()
        {
            CrashState& state = State();

            ProvokeReentryForSelfCheck();
            CaptureLoadedModules();

            TextBuffer out{state.Record, kRecordCapacity, 0u, false};
            AppendText(out, "\n=== Editor crash ");
            AppendTimestamp(out);
            AppendText(out, " ===\n");
            AppendLoadedModules(out);
            AppendFaultThread(out);
            AppendExceptionFacts(out);
            AppendContextRegisters(out);
            AppendFaultingStackWords(out);
            AppendFilterStack(out);

            state.RecordFileWritten = AppendToRecordFile(state.Record, out.Size);

            // The record is on disk before the dump is attempted: the dump is the part that takes
            // the loader lock and cannot be bounded, and it must not be able to cost the record.
            ::SetEvent(state.RecordWrittenEvent);

            BuildDumpPath();
            TextBuffer dumpStatus{state.Record, kRecordCapacity, 0u, false};
            AppendMiniDumpLine(dumpStatus, state.DumpPathW);

            char line[kPathChars]{};
            TextBuffer lineBuffer{line, static_cast<unsigned>(sizeof(line)), 0u, false};
            AppendText(lineBuffer, "MiniDump: ");
            AppendText(lineBuffer, dumpStatus.Data != nullptr ? dumpStatus.Data : "");
            AppendChar(lineBuffer, '\n');
            AppendToRecordFile(line, lineBuffer.Size);

            ::SetEvent(state.RecordDoneEvent);
        }

        DWORD WINAPI CrashRecordThread(void*)
        {
            const HANDLE faultEvent = State().FaultEvent;
            ::WaitForSingleObject(faultEvent, INFINITE);
            WriteRecordOnHealthyStack();
            return 0;
        }

        // ---- the fault path ----------------------------------------------------

        bool IsRecordedCode(DWORD code)
        {
            return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION
                || code == EXCEPTION_STACK_OVERFLOW || code == EXCEPTION_INT_DIVIDE_BY_ZERO;
        }

        void StashFaultReport(EXCEPTION_POINTERS* exceptionPointers)
        {
            CrashState& state = State();
            const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;

            state.Exception = *record;
            state.HasContext = exceptionPointers->ContextRecord != nullptr;
            if (state.HasContext)
            {
                state.Context = *exceptionPointers->ContextRecord;
            }

            // Identity and the filter stack are thread-local answers: they can only be taken here,
            // by the thread that faulted, and must be read from static storage afterwards.
            state.FaultThreadId = ::GetCurrentThreadId();
            const ThreadIdentity identity = CaptureCurrentThreadIdentity();
            state.FaultLogThreadId = identity.LogThreadId;
            state.FaultRoleName = identity.RoleName != nullptr ? identity.RoleName : "none";
            state.FilterFrameCount =
                ::CaptureStackBackTrace(0, kFilterFrameCount, state.FilterFrames, nullptr);
        }

        LONG WINAPI EditorUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
        {
            CrashState& state = State();
            if (!state.Installed || exceptionPointers == nullptr
                || exceptionPointers->ExceptionRecord == nullptr)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (!IsRecordedCode(exceptionPointers->ExceptionRecord->ExceptionCode))
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            // One fault path per process. A second fault - inside the record path itself, or on
            // another thread while the first is still recording - is turned away here instead of
            // nesting on storage and stacks that are already in use.
            if (::InterlockedCompareExchange(&state.FaultPathLatch, 1, 0) != 0)
            {
                ::InterlockedIncrement(&state.Rejected);
                // Waiting for the record this very thread is writing would only deadlock it.
                if (::GetCurrentThreadId() != state.RecordThreadId)
                {
                    ::WaitForSingleObject(state.RecordWrittenEvent, kRecordWaitMs);
                }
                return EXCEPTION_CONTINUE_SEARCH;
            }
            ::InterlockedIncrement(&state.Admitted);

            if (state.RecordThread == nullptr)
            {
                // No prepared stack to hand the fault to: waiting here would only add a stall to
                // the crash, so let the process go down as it would have without the filter.
                return EXCEPTION_CONTINUE_SEARCH;
            }

            // A thread that has overflowed its stack has no room left for anything. Restoring the
            // guard page gives it one page back, which is enough to reach the hand-off.
            if (exceptionPointers->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
            {
                _resetstkoflw();
            }

            StashFaultReport(exceptionPointers);

            // Everything below is a kernel transition on already-created handles: no allocation, no
            // lock the faulting thread could be holding, no formatting on this stack.
            ::SetEvent(state.FaultEvent);
            ::WaitForSingleObject(state.RecordWrittenEvent, kRecordWaitMs);
            ::WaitForSingleObject(state.RecordDoneEvent, kDumpWaitMs);

            return EXCEPTION_CONTINUE_SEARCH;
        }

        // ---- installation ------------------------------------------------------

        void PreparePaths()
        {
            CrashState& state = State();

            wchar_t modulePath[kPathChars]{};
            const DWORD length = ::GetModuleFileNameW(nullptr, modulePath, kPathChars);
            unsigned directoryLength = 0;
            if (length != 0 && length < kPathChars)
            {
                for (unsigned index = 0; index < length; ++index)
                {
                    if (modulePath[index] == L'\\' || modulePath[index] == L'/')
                    {
                        directoryLength = index + 1;
                    }
                }
            }

            for (unsigned index = 0; index < directoryLength && index < kPathChars - 1; ++index)
            {
                state.DirectoryW[index] = modulePath[index];
            }
            state.DirectoryW[directoryLength] = L'\0';

            unsigned pathIndex = 0;
            for (; pathIndex < directoryLength && pathIndex < kPathChars - 1; ++pathIndex)
            {
                state.RecordPathW[pathIndex] = state.DirectoryW[pathIndex];
            }

            const wchar_t* name = L"ed_crash.log";
            for (unsigned index = 0; name[index] != L'\0' && pathIndex < kPathChars - 1; ++index)
            {
                state.RecordPathW[pathIndex++] = name[index];
            }
            state.RecordPathW[pathIndex] = L'\0';

            const int narrowLength = ::WideCharToMultiByte(
                CP_UTF8, 0, state.RecordPathW, -1, state.RecordPathA, kPathChars, nullptr, nullptr);
            if (narrowLength <= 0)
            {
                state.RecordPathA[0] = '\0';
            }
        }

        void ReserveFaultStackForCurrentThread()
        {
            // Only this thread can be prepared from here; a thread created elsewhere keeps the
            // system default and relies on the hand-off being as small as it is.
            ULONG guarantee = kFaultStackGuaranteeBytes;
            ::SetThreadStackGuarantee(&guarantee);
        }

        void StartRecordThread()
        {
            CrashState& state = State();

            state.FaultEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            state.RecordWrittenEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            state.RecordDoneEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (state.FaultEvent == nullptr || state.RecordWrittenEvent == nullptr
                || state.RecordDoneEvent == nullptr)
            {
                return;
            }

            DWORD threadId = 0;
            state.RecordThread = ::CreateThread(
                nullptr, kRecordThreadStackBytes, CrashRecordThread, nullptr, 0, &threadId);
            if (state.RecordThread == nullptr)
            {
                return;
            }

            // The record has to run while the faulting thread is still alive, whatever priority it
            // was given.
            ::SetThreadPriority(state.RecordThread, THREAD_PRIORITY_TIME_CRITICAL);
            state.RecordThreadId = threadId;
        }
    }
#endif

    std::string InstallEditorCrashDiagnostics()
    {
#if defined(_WIN32)
        CrashState& state = State();
        if (state.Installed)
        {
            return std::string(state.RecordPathA);
        }

        PreparePaths();
        ReserveFaultStackForCurrentThread();
        StartRecordThread();
        state.Installed = true;
        ::SetUnhandledExceptionFilter(EditorUnhandledExceptionFilter);
        return std::string(state.RecordPathA);
#else
        return std::string();
#endif
    }

    CrashRecordSelfCheckResult RunCrashRecordSelfCheck(bool provokeReentry)
    {
        CrashRecordSelfCheckResult result;
#if defined(_WIN32)
        CrashState& state = State();
        if (!state.Installed)
        {
            return result;
        }

        result.RecordPath.assign(state.RecordPathA);
        ::InterlockedExchange(&state.SelfCheckReentry, provokeReentry ? 1 : 0);

        EXCEPTION_RECORD record{};
        record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        record.ExceptionAddress = reinterpret_cast<void*>(&RunCrashRecordSelfCheck);
        record.NumberParameters = 2;
        record.ExceptionInformation[0] = 1;
        record.ExceptionInformation[1] = reinterpret_cast<ULONG_PTR>(&state.Record);

        CONTEXT context;
        ::RtlCaptureContext(&context);
        EXCEPTION_POINTERS pointers{&record, &context};

        // Waits for the record to be on disk, then returns: the caller is still alive and keeps it.
        EditorUnhandledExceptionFilter(&pointers);

        result.RecordWritten = state.RecordFileWritten;
        result.Admitted = ::InterlockedCompareExchange(&state.Admitted, 0, 0);
        result.Rejected = ::InterlockedCompareExchange(&state.Rejected, 0, 0);
#endif
        return result;
    }
}
