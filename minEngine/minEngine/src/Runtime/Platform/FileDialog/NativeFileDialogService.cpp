#include "NativeFileDialogService.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <nfd.h>

namespace minEngine
{
    struct NfdU8FilterItem
    {
        const char* Name = nullptr;
        const char* Spec = nullptr;
    };

    void NativeFileDialogService::NfdFilterBinding::BuildFrom(const std::vector<FileDialogFilter>& filters)
    {
        m_LabelStorage.clear();
        m_SpecStorage.clear();
        m_NfdFilters.clear();

        m_LabelStorage.reserve(filters.size());
        m_SpecStorage.reserve(filters.size());
        m_NfdFilters.reserve(filters.size());

        for (const FileDialogFilter& filter : filters)
        {
            m_LabelStorage.push_back(filter.Label);
            m_SpecStorage.push_back(filter.ExtensionSpec);
            m_NfdFilters.push_back(
                NfdU8FilterItem{m_LabelStorage.back().c_str(), m_SpecStorage.back().c_str()});
        }
    }

    const NfdU8FilterItem* NativeFileDialogService::NfdFilterBinding::Data() const
    {
        return m_NfdFilters.empty() ? nullptr : m_NfdFilters.data();
    }

    std::size_t NativeFileDialogService::NfdFilterBinding::Count() const
    {
        return m_NfdFilters.size();
    }

    NativeFileDialogService::NativeFileDialogService(bool nfdReady)
        : m_NfdReady(nfdReady)
    {
    }

    FileDialogResult NativeFileDialogService::MakeCancelledResult() const
    {
        return FileDialogResult{};
    }

    std::string NativeFileDialogService::ResolveDefaultPath(const FileDialogRequest& request) const
    {
        if (request.InitialDirectory.empty())
        {
            return std::string();
        }

        return request.InitialDirectory.generic_string();
    }

    FileDialogResult NativeFileDialogService::OpenFiles(const FileDialogRequest& request)
    {
        if (!m_NfdReady)
        {
            ME_CORE_ERROR("OpenFiles: NFD is not initialized.");
            return MakeCancelledResult();
        }

        NfdFilterBinding filterBinding;
        filterBinding.BuildFrom(request.Filters);

        std::vector<nfdu8filteritem_t> nfdFilters;
        nfdFilters.reserve(filterBinding.Count());
        for (std::size_t filterIndex = 0; filterIndex < filterBinding.Count(); ++filterIndex)
        {
            const NfdU8FilterItem* item = filterBinding.Data() + filterIndex;
            nfdFilters.push_back(nfdu8filteritem_t{item->Name, item->Spec});
        }

        const std::string defaultPath = ResolveDefaultPath(request);

        nfdopendialogu8args_t args{};
        args.filterList = nfdFilters.empty() ? nullptr : nfdFilters.data();
        args.filterCount = static_cast<nfdfiltersize_t>(nfdFilters.size());
        args.defaultPath = defaultPath.empty() ? nullptr : defaultPath.c_str();

        FileDialogResult result;

        if (request.bAllowMultiple)
        {
            const nfdpathset_t* pathSet = nullptr;
            const nfdresult_t dialogResult = NFD_OpenDialogMultipleU8_With(&pathSet, &args);
            if (dialogResult == NFD_CANCEL)
            {
                return MakeCancelledResult();
            }

            if (dialogResult != NFD_OKAY || pathSet == nullptr)
            {
                ME_CORE_ERROR("OpenFiles (multiple): {}", NFD_GetError());
                return MakeCancelledResult();
            }

            nfdpathsetsize_t pathCount = 0;
            if (NFD_PathSet_GetCount(pathSet, &pathCount) != NFD_OKAY)
            {
                ME_CORE_ERROR("OpenFiles (multiple): failed to read path set count.");
                NFD_PathSet_Free(pathSet);
                return MakeCancelledResult();
            }

            result.Paths.reserve(pathCount);
            for (nfdpathsetsize_t pathIndex = 0; pathIndex < pathCount; ++pathIndex)
            {
                nfdu8char_t* path = nullptr;
                if (NFD_PathSet_GetPathU8(pathSet, pathIndex, &path) != NFD_OKAY || path == nullptr)
                {
                    continue;
                }

                result.Paths.emplace_back(path);
                NFD_PathSet_FreePathU8(path);
            }

            NFD_PathSet_Free(pathSet);
            result.bCancelled = result.Paths.empty();
            return result;
        }

        nfdu8char_t* singlePath = nullptr;
        const nfdresult_t dialogResult = NFD_OpenDialogU8_With(&singlePath, &args);
        if (dialogResult == NFD_CANCEL)
        {
            return MakeCancelledResult();
        }

        if (dialogResult != NFD_OKAY || singlePath == nullptr)
        {
            ME_CORE_ERROR("OpenFiles: {}", NFD_GetError());
            return MakeCancelledResult();
        }

        result.Paths.emplace_back(singlePath);
        NFD_FreePathU8(singlePath);
        result.bCancelled = false;
        return result;
    }

    FileDialogResult NativeFileDialogService::SaveFile(
        const FileDialogRequest& request,
        std::string_view defaultFileName)
    {
        if (!m_NfdReady)
        {
            ME_CORE_ERROR("SaveFile: NFD is not initialized.");
            return MakeCancelledResult();
        }

        NfdFilterBinding filterBinding;
        filterBinding.BuildFrom(request.Filters);

        std::vector<nfdu8filteritem_t> nfdFilters;
        nfdFilters.reserve(filterBinding.Count());
        for (std::size_t filterIndex = 0; filterIndex < filterBinding.Count(); ++filterIndex)
        {
            const NfdU8FilterItem* item = filterBinding.Data() + filterIndex;
            nfdFilters.push_back(nfdu8filteritem_t{item->Name, item->Spec});
        }

        const std::string defaultPath = ResolveDefaultPath(request);
        const std::string defaultName(defaultFileName);

        nfdsavedialogu8args_t args{};
        args.filterList = nfdFilters.empty() ? nullptr : nfdFilters.data();
        args.filterCount = static_cast<nfdfiltersize_t>(nfdFilters.size());
        args.defaultPath = defaultPath.empty() ? nullptr : defaultPath.c_str();
        args.defaultName = defaultName.empty() ? nullptr : defaultName.c_str();

        nfdu8char_t* outPath = nullptr;
        const nfdresult_t dialogResult = NFD_SaveDialogU8_With(&outPath, &args);
        if (dialogResult == NFD_CANCEL)
        {
            return MakeCancelledResult();
        }

        if (dialogResult != NFD_OKAY || outPath == nullptr)
        {
            ME_CORE_ERROR("SaveFile: {}", NFD_GetError());
            return MakeCancelledResult();
        }

        FileDialogResult result;
        result.Paths.emplace_back(outPath);
        NFD_FreePathU8(outPath);
        result.bCancelled = false;
        return result;
    }

    FileDialogResult NativeFileDialogService::SelectFolder(const FileDialogRequest& request)
    {
        if (!m_NfdReady)
        {
            ME_CORE_ERROR("SelectFolder: NFD is not initialized.");
            return MakeCancelledResult();
        }

        const std::string defaultPath = ResolveDefaultPath(request);

        nfdpickfolderu8args_t args{};
        args.defaultPath = defaultPath.empty() ? nullptr : defaultPath.c_str();

        nfdu8char_t* outPath = nullptr;
        const nfdresult_t dialogResult = NFD_PickFolderU8_With(&outPath, &args);
        if (dialogResult == NFD_CANCEL)
        {
            return MakeCancelledResult();
        }

        if (dialogResult != NFD_OKAY || outPath == nullptr)
        {
            ME_CORE_ERROR("SelectFolder: {}", NFD_GetError());
            return MakeCancelledResult();
        }

        FileDialogResult result;
        result.Paths.emplace_back(outPath);
        NFD_FreePathU8(outPath);
        result.bCancelled = false;
        return result;
    }
}
