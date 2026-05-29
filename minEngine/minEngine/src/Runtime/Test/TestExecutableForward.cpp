#include "TestExecutableForward.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace minEngine
{
    namespace
    {
        std::filesystem::path GetSiblingTestsExecutablePath()
        {
#if defined(_WIN32)
            wchar_t modulePath[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                return {};
            }
            std::filesystem::path executablePath(modulePath);
#else
            std::filesystem::path executablePath = std::filesystem::current_path() / "minEngineTests";
#endif
            executablePath.replace_filename("minEngineTests.exe");
            return executablePath;
        }

        std::wstring Utf8ToWide(const std::string& text)
        {
#if defined(_WIN32)
            if (text.empty())
            {
                return {};
            }

            const int required =
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
            if (required <= 0)
            {
                return {};
            }

            std::wstring wide(static_cast<size_t>(required), L'\0');
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                wide.data(),
                required);
            return wide;
#else
            (void)text;
            return {};
#endif
        }
    }

    int ForwardToMinEngineTestsExecutable(int argc, char** argv)
    {
        const std::filesystem::path testsExecutable = GetSiblingTestsExecutablePath();
        if (testsExecutable.empty() || !std::filesystem::exists(testsExecutable))
        {
            std::fprintf(
                stderr,
                "Editor: minEngineTests.exe not found next to Editor ('%s').\n"
                "Build target minEngineTests and run tests from minEngine/bin.\n",
                testsExecutable.string().c_str());
            return 2;
        }

        std::fprintf(
            stderr,
            "Note: Editor.exe test mode forwards to minEngineTests.exe (see TEST-F02).\n");

        std::vector<std::string> argumentStrings;
        argumentStrings.push_back(testsExecutable.string());
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] != nullptr)
            {
                argumentStrings.push_back(argv[argIndex]);
            }
        }

        std::vector<std::wstring> wideStorage;
        wideStorage.reserve(argumentStrings.size());
        for (const std::string& argument : argumentStrings)
        {
            wideStorage.push_back(Utf8ToWide(argument));
        }

#if defined(_WIN32)
        std::wstring commandLine = L"\"" + wideStorage.front() + L"\"";
        for (size_t argIndex = 1; argIndex < wideStorage.size(); ++argIndex)
        {
            commandLine += L" \"";
            commandLine += wideStorage[argIndex];
            commandLine += L"\"";
        }

        std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};

        if (!CreateProcessW(
                nullptr,
                mutableCommandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                nullptr,
                &startupInfo,
                &processInfo))
        {
            std::fprintf(stderr, "Editor: failed to launch minEngineTests.exe.\n");
            return 2;
        }

        WaitForSingleObject(processInfo.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(processInfo.hProcess, &exitCode);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return static_cast<int>(exitCode);
#else
        return 2;
#endif
    }
}
