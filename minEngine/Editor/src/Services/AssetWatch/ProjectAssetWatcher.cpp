#include "Services/AssetWatch/ProjectAssetWatcher.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetTypeRegistry.h"

#include <algorithm>

namespace minEngine
{
    void ProjectAssetWatcher::Register(IEditorContext& context)
    {
        m_Context = &context;
    }

    void ProjectAssetWatcher::Shutdown()
    {
        StopWatching();
        m_Context = nullptr;
    }

    void ProjectAssetWatcher::StartWatching(const std::filesystem::path& projectContentRoot)
    {
        StopWatching();

        if (projectContentRoot.empty())
        {
            ME_CORE_WARN("ProjectAssetWatcher: content root is empty; watcher not started.");
            return;
        }

        std::error_code errorCode;
        if (!std::filesystem::exists(projectContentRoot, errorCode)
            || !std::filesystem::is_directory(projectContentRoot, errorCode))
        {
            ME_CORE_WARN(
                "ProjectAssetWatcher: content root is not a directory: {}",
                projectContentRoot.string());
            return;
        }

        m_WatchedRoot = std::filesystem::weakly_canonical(projectContentRoot);

        m_FileWatcher = new efsw::FileWatcher();
        m_WatchId = m_FileWatcher->addWatch(m_WatchedRoot.string(), this, true);
        if (m_WatchId < 0)
        {
            ME_CORE_ERROR(
                "ProjectAssetWatcher: failed to watch '{}': {}",
                m_WatchedRoot.string(),
                efsw::Errors::Log::getLastErrorLog());
            delete m_FileWatcher;
            m_FileWatcher = nullptr;
            m_WatchId = -1;
            return;
        }

        m_FileWatcher->watch();
        m_WatchStarted = true;

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_PendingEvents.clear();
            m_HasPendingDebounce = false;
            m_DebounceElapsedSeconds = 0.0f;
        }

        ME_CORE_INFO("ProjectAssetWatcher: watching '{}'", m_WatchedRoot.string());
    }

    void ProjectAssetWatcher::StopWatching()
    {
        if (m_FileWatcher != nullptr)
        {
            if (m_WatchId >= 0)
            {
                m_FileWatcher->removeWatch(m_WatchId);
                m_WatchId = -1;
            }

            delete m_FileWatcher;
            m_FileWatcher = nullptr;
        }

        m_WatchStarted = false;
        m_WatchedRoot.clear();

        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_PendingEvents.clear();
            m_HasPendingDebounce = false;
            m_DebounceElapsedSeconds = 0.0f;
        }

        if (m_SuppressedBatchCount > 0)
        {
            ME_CORE_DEBUG(
                "ProjectAssetWatcher: discarded {} debounced batches while external sync was suppressed.",
                m_SuppressedBatchCount);
            m_SuppressedBatchCount = 0;
        }
    }

    void ProjectAssetWatcher::Tick(float deltaTime)
    {
        if (!m_WatchStarted)
        {
            return;
        }

        bool shouldProcess = false;
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            if (!m_HasPendingDebounce)
            {
                return;
            }

            m_DebounceElapsedSeconds += deltaTime;
            if (m_DebounceElapsedSeconds >= kDebounceSeconds)
            {
                shouldProcess = true;
                m_HasPendingDebounce = false;
                m_DebounceElapsedSeconds = 0.0f;
            }
        }

        if (shouldProcess)
        {
            ProcessPendingActions();
        }
    }

    void ProjectAssetWatcher::handleFileAction(
        efsw::WatchID watchId,
        const std::string& dir,
        const std::string& filename,
        efsw::Action action,
        std::string oldFilename)
    {
        (void)watchId;

        if (filename.empty())
        {
            return;
        }

        const std::filesystem::path namePath(filename);
        if (ShouldIgnoreFilename(namePath))
        {
            return;
        }

        const std::filesystem::path absolutePath = CombineWatchPath(dir, filename);

        std::error_code errorCode;
        if (std::filesystem::is_directory(absolutePath, errorCode))
        {
            RequestDebouncedFullRescan();
            return;
        }

        if (action == efsw::Action::Delete)
        {
            if (ShouldIgnoreMetaOnly(namePath))
            {
                return;
            }

            PendingFileEvent event;
            event.Kind = PendingActionKind::Unregister;
            event.AbsolutePath = absolutePath;
            EnqueueFileAction(std::move(event));
            return;
        }

        if (action == efsw::Action::Moved)
        {
            if (oldFilename.empty())
            {
                return;
            }

            const std::filesystem::path oldNamePath(oldFilename);
            if (ShouldIgnoreFilename(oldNamePath))
            {
                return;
            }

            if (ShouldIgnoreMetaOnly(namePath) || ShouldIgnoreMetaOnly(oldNamePath))
            {
                return;
            }

            PendingFileEvent event;
            event.Kind = PendingActionKind::Move;
            event.AbsolutePath = absolutePath;
            event.OldAbsolutePath = CombineWatchPath(dir, oldFilename);
            EnqueueFileAction(std::move(event));
            return;
        }

        if (action == efsw::Action::Add || action == efsw::Action::Modified)
        {
            if (ShouldIgnoreMetaOnly(namePath))
            {
                return;
            }

            PendingFileEvent event;
            event.Kind = PendingActionKind::RegisterOrUpdate;
            event.AbsolutePath = absolutePath;
            EnqueueFileAction(std::move(event));
        }
    }

    void ProjectAssetWatcher::EnqueueFileAction(PendingFileEvent event)
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_PendingEvents.push_back(std::move(event));
        m_HasPendingDebounce = true;
        m_DebounceElapsedSeconds = 0.0f;
    }

    void ProjectAssetWatcher::RequestDebouncedFullRescan()
    {
        PendingFileEvent event;
        event.Kind = PendingActionKind::RequestFullRescan;
        EnqueueFileAction(std::move(event));
    }

    void ProjectAssetWatcher::ProcessPendingActions()
    {
        if (AssetManager::HasInstance() && AssetManager::Get().IsExternalSyncSuppressed())
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_PendingEvents.clear();
            ++m_SuppressedBatchCount;
            return;
        }

        std::vector<PendingFileEvent> events;
        {
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            events.swap(m_PendingEvents);
        }

        if (events.empty() || m_WatchedRoot.empty())
        {
            return;
        }

        size_t directoryRescanRequests = 0;
        size_t fileLevelEvents = 0;
        for (const PendingFileEvent& event : events)
        {
            if (event.Kind == PendingActionKind::RequestFullRescan)
            {
                ++directoryRescanRequests;
            }
            else
            {
                ++fileLevelEvents;
            }
        }

        if (directoryRescanRequests >= kDirectoryEventThreshold
            || fileLevelEvents > kFullRescanFileThreshold)
        {
            RunFullRescan();
            return;
        }

        const size_t processCount = std::min(events.size(), kMaxEventsPerProcessBatch);
        for (size_t index = 0; index < processCount; ++index)
        {
            const PendingFileEvent& event = events[index];
            switch (event.Kind)
            {
            case PendingActionKind::RequestFullRescan:
                RunFullRescan();
                return;
            case PendingActionKind::RegisterOrUpdate:
                ProcessRegisterOrUpdate(event.AbsolutePath);
                break;
            case PendingActionKind::Unregister:
                ProcessUnregister(event.AbsolutePath);
                break;
            case PendingActionKind::Move:
                ProcessMove(event.OldAbsolutePath, event.AbsolutePath);
                break;
            }
        }

        if (events.size() > processCount)
        {
            std::vector<PendingFileEvent> remaining(
                events.begin() + static_cast<std::ptrdiff_t>(processCount),
                events.end());
            std::lock_guard<std::mutex> lock(m_QueueMutex);
            m_PendingEvents.insert(
                m_PendingEvents.begin(),
                remaining.begin(),
                remaining.end());
            m_HasPendingDebounce = true;
            m_DebounceElapsedSeconds = 0.0f;
        }
    }

    bool ProjectAssetWatcher::ShouldIgnoreFilename(const std::filesystem::path& filename) const
    {
        const std::string stem = filename.stem().string();
        if (!stem.empty() && stem.front() == '~')
        {
            return true;
        }

        const std::string extension = filename.extension().string();
        if (extension == ".tmp" || extension == ".bak")
        {
            return true;
        }

        return false;
    }

    bool ProjectAssetWatcher::ShouldIgnoreMetaOnly(const std::filesystem::path& filename) const
    {
        return filename.extension() == ".meta";
    }

    std::filesystem::path ProjectAssetWatcher::CombineWatchPath(
        const std::string& dir,
        const std::string& filename) const
    {
        return std::filesystem::path(dir) / filename;
    }

    bool ProjectAssetWatcher::IsUnderWatchedRoot(const std::filesystem::path& absolutePath) const
    {
        if (m_WatchedRoot.empty() || absolutePath.empty())
        {
            return false;
        }

        std::error_code errorCode;
        const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(m_WatchedRoot, errorCode);
        if (errorCode)
        {
            return false;
        }

        const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(absolutePath, errorCode);
        if (errorCode)
        {
            return false;
        }

        auto mismatchPair =
            std::mismatch(canonicalRoot.begin(), canonicalRoot.end(), canonicalPath.begin());
        return mismatchPair.first == canonicalRoot.end();
    }

    void ProjectAssetWatcher::ProcessRegisterOrUpdate(const std::filesystem::path& absolutePath)
    {
        if (!IsUnderWatchedRoot(absolutePath))
        {
            return;
        }

        std::error_code errorCode;
        if (!std::filesystem::is_regular_file(absolutePath, errorCode))
        {
            return;
        }

        const std::string assetTypeId = AssetTypeRegistry::Get().InferAssetTypeFromExtension(absolutePath);
        if (assetTypeId.empty())
        {
            return;
        }

        AssetManager::Get().RegisterAsset(absolutePath.string(), assetTypeId);
    }

    void ProjectAssetWatcher::ProcessUnregister(const std::filesystem::path& absolutePath)
    {
        if (!IsUnderWatchedRoot(absolutePath))
        {
            return;
        }

        AssetManager& assetManager = AssetManager::Get();

        std::string errorMessage;
        if (!assetManager.UnregisterAsset(absolutePath.string(), errorMessage))
        {
            ME_CORE_DEBUG(
                "ProjectAssetWatcher: UnregisterAsset for '{}': {}",
                absolutePath.string(),
                errorMessage);
        }
    }

    void ProjectAssetWatcher::ProcessMove(
        const std::filesystem::path& oldAbsolutePath,
        const std::filesystem::path& newAbsolutePath)
    {
        if (!IsUnderWatchedRoot(oldAbsolutePath) || !IsUnderWatchedRoot(newAbsolutePath))
        {
            return;
        }

        AssetManager& assetManager = AssetManager::Get();
        const std::filesystem::path oldExtension = oldAbsolutePath.extension();
        const std::filesystem::path newExtension = newAbsolutePath.extension();

        if (oldExtension != newExtension)
        {
            std::string errorMessage;
            assetManager.UnregisterAsset(oldAbsolutePath.string(), errorMessage);
            ProcessRegisterOrUpdate(newAbsolutePath);
            return;
        }

        std::string moveError;
        if (assetManager.MoveAsset(oldAbsolutePath.string(), newAbsolutePath.string(), moveError))
        {
            return;
        }

        ME_CORE_DEBUG(
            "ProjectAssetWatcher: MoveAsset failed ({}); falling back to unregister + register.",
            moveError);

        std::string unregisterError;
        assetManager.UnregisterAsset(oldAbsolutePath.string(), unregisterError);
        ProcessRegisterOrUpdate(newAbsolutePath);
    }

    void ProjectAssetWatcher::RunFullRescan()
    {
        if (m_WatchedRoot.empty())
        {
            return;
        }

        ME_CORE_INFO(
            "ProjectAssetWatcher: running full ScanAssets on '{}'",
            m_WatchedRoot.string());
        AssetManager::Get().ScanAssets(m_WatchedRoot);
    }
}
