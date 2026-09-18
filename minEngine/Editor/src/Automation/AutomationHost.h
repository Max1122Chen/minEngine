#pragma once

#include "Automation/AutomationDispatcher.h"
#include "Automation/AutomationPipeServer.h"
#include "Automation/CapabilityRegistry.h"
#include "Automation/EditorAutomationSurface.h"
#include "Services/Scene/SceneEditorServices.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace minEngine
{
    class IEditorContext;

    class AutomationHost
    {
    public:
        explicit AutomationHost(SceneEditorServices& sceneServices);
        ~AutomationHost();

        AutomationHost(const AutomationHost&) = delete;
        AutomationHost& operator=(const AutomationHost&) = delete;

        /**
         * Listen on pipe, then write ready-file (if provided).
         * Call only after project (+ optional open-scene) is ready.
         */
        bool Start(
            const std::string& pipeName,
            const std::optional<std::filesystem::path>& readyFilePath);

        void Stop();

        /**
         * Main-thread pump: if an IO-thread exchange is pending, dispatch it and complete the response.
         * Scheduling invariant (MVP): single in-flight request — at most one PendingExchange;
         * IO blocks until this Tick finishes. Not a request queue; upgrade later if needed.
         */
        void Tick(IEditorContext& context);

        bool IsRunning() const { return m_Running.load(); }

    private:
        struct PendingExchange
        {
            std::string RequestJson;
            std::string ResponseJson;
            bool Ready = false;
            bool Cancelled = false;
        };

        void IoThreadMain();
        bool WriteReadyFile(const std::optional<std::filesystem::path>& readyFilePath) const;

        CapabilityRegistry m_Capabilities;
        SceneEditorServices& m_SceneServices;
        EditorAutomationSurface m_Surface;
        AutomationDispatcher m_Dispatcher;
        AutomationPipeServer m_PipeServer;

        std::thread m_IoThread;
        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_StopRequested{false};

        std::mutex m_Mutex;
        std::condition_variable m_IoCv;
        std::condition_variable m_MainCv;
        PendingExchange* m_Pending = nullptr;

        std::wstring m_PipePathWide;
        std::string m_PipePathNarrow;
    };
}
