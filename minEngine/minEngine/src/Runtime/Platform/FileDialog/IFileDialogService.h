#pragma once

#include "FileDialogTypes.h"

#include <string_view>

namespace minEngine
{
    class IFileDialogService
    {
    public:
        virtual ~IFileDialogService() = default;

        virtual FileDialogResult OpenFiles(const FileDialogRequest& request) = 0;
        virtual FileDialogResult SaveFile(
            const FileDialogRequest& request,
            std::string_view defaultFileName = {}) = 0;
        virtual FileDialogResult SelectFolder(const FileDialogRequest& request) = 0;
    };
}
