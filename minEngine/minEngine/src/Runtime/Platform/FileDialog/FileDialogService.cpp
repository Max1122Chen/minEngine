#include "FileDialogService.h"

#include "NativeFileDialogService.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <nfd.h>

namespace minEngine
{
    FileDialogService* FileDialogService::s_Instance = nullptr;

    void FileDialogService::SetInstance(FileDialogService* instance)
    {
        s_Instance = instance;
    }

    FileDialogService& FileDialogService::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "FileDialogService is not initialized");
        return *s_Instance;
    }

    bool FileDialogService::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void FileDialogService::Initialize()
    {
        m_NfdReady = false;
        m_Implementation.reset();

        const nfdresult_t initResult = NFD_Init();
        if (initResult != NFD_OKAY)
        {
            ME_CORE_ERROR("FileDialogService: NFD_Init failed: {}", NFD_GetError());
            m_Implementation = std::make_unique<NativeFileDialogService>(false);
            return;
        }

        m_NfdReady = true;
        m_Implementation = std::make_unique<NativeFileDialogService>(true);
        ME_CORE_INFO("FileDialogService initialized (NFD).");
    }

    void FileDialogService::Shutdown()
    {
        m_Implementation.reset();

        if (m_NfdReady)
        {
            NFD_Quit();
            m_NfdReady = false;
            ME_CORE_INFO("FileDialogService shutdown (NFD).");
        }
    }

    IFileDialogService& FileDialogService::GetImplementation()
    {
        ME_ASSERT(m_Implementation != nullptr, "FileDialogService implementation is not available");
        return *m_Implementation;
    }

    const IFileDialogService& FileDialogService::GetImplementation() const
    {
        ME_ASSERT(m_Implementation != nullptr, "FileDialogService implementation is not available");
        return *m_Implementation;
    }
}
