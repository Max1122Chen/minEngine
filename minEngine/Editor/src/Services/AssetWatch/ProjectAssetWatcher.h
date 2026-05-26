#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

#include <efsw/efsw.hpp>

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace minEngine
{
    class IEditorContext;

    class ProjectAssetWatcher : public EditorServiceModule, public efsw::FileWatchListener
    {
    public:
        static constexpr const char* kModuleId = "ProjectAssetWatcher";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;
        void Tick(float deltaTime) override;

        void StartWatching(const std::filesystem::path& projectContentRoot);
        void StopWatching();

    private:
        enum class PendingActionKind
        {
            RegisterOrUpdate,
            Unregister,
            Move,
            RequestFullRescan,
        };

        struct PendingFileEvent
        {
            PendingActionKind Kind = PendingActionKind::RegisterOrUpdate;
            std::filesystem::path AbsolutePath;
            std::filesystem::path OldAbsolutePath;
        };

        static constexpr float kDebounceSeconds = 0.4f;
        static constexpr size_t kMaxEventsPerProcessBatch = 64;
        static constexpr size_t kFullRescanFileThreshold = 32;
        static constexpr size_t kDirectoryEventThreshold = 1;

        void handleFileAction(
            efsw::WatchID watchId,
            const std::string& dir,
            const std::string& filename,
            efsw::Action action,
            std::string oldFilename) override;

        void EnqueueFileAction(PendingFileEvent event);
        void ProcessPendingActions();
        bool ShouldIgnoreFilename(const std::filesystem::path& filename) const;
        bool ShouldIgnoreMetaOnly(const std::filesystem::path& filename) const;
        std::filesystem::path CombineWatchPath(const std::string& dir, const std::string& filename) const;
        bool IsUnderWatchedRoot(const std::filesystem::path& absolutePath) const;
        void RequestDebouncedFullRescan();
        void ProcessRegisterOrUpdate(const std::filesystem::path& absolutePath);
        void ProcessUnregister(const std::filesystem::path& absolutePath);
        void ProcessMove(const std::filesystem::path& oldAbsolutePath, const std::filesystem::path& newAbsolutePath);
        void RunFullRescan();

        IEditorContext* m_Context = nullptr;
        std::filesystem::path m_WatchedRoot;
        efsw::FileWatcher* m_FileWatcher = nullptr;
        efsw::WatchID m_WatchId = -1;
        bool m_WatchStarted = false;

        std::mutex m_QueueMutex;
        std::vector<PendingFileEvent> m_PendingEvents;
        float m_DebounceElapsedSeconds = 0.0f;
        bool m_HasPendingDebounce = false;
        size_t m_SuppressedBatchCount = 0;
    };
}
