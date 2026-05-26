#pragma once

#include "Core.h"

#include <filesystem>
#include <string>
#include <vector>

namespace minEngine
{
    struct FileDialogFilter
    {
        std::string Label;
        std::string ExtensionSpec;
    };

    struct FileDialogRequest
    {
        std::string Title;
        std::filesystem::path InitialDirectory;
        std::vector<FileDialogFilter> Filters;
        bool bAllowMultiple = false;
    };

    struct FileDialogResult
    {
        bool bCancelled = true;
        std::vector<std::filesystem::path> Paths;
    };
}
