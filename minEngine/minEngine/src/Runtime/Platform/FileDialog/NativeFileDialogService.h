#pragma once

#include "IFileDialogService.h"

#include <cstddef>
#include <string>
#include <vector>

namespace minEngine
{
    struct NfdU8FilterItem;
    class NativeFileDialogService : public IFileDialogService
    {
    public:
        explicit NativeFileDialogService(bool nfdReady);

        FileDialogResult OpenFiles(const FileDialogRequest& request) override;
        FileDialogResult SaveFile(
            const FileDialogRequest& request,
            std::string_view defaultFileName = {}) override;
        FileDialogResult SelectFolder(const FileDialogRequest& request) override;

    private:
        class NfdFilterBinding
        {
        public:
            void BuildFrom(const std::vector<FileDialogFilter>& filters);

            const NfdU8FilterItem* Data() const;
            std::size_t Count() const;

        private:
            std::vector<std::string> m_LabelStorage;
            std::vector<std::string> m_SpecStorage;
            std::vector<NfdU8FilterItem> m_NfdFilters;
        };

        FileDialogResult MakeCancelledResult() const;
        std::string ResolveDefaultPath(const FileDialogRequest& request) const;

        bool m_NfdReady = false;
    };
}
