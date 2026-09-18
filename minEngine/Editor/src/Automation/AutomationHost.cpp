#include "Automation/AutomationHost.h"

#include "Automation/AutomationProtocol.h"
#include "Core.h"
#include "Shell/IEditorContext.h"

#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace minEngine
{
    AutomationHost::AutomationHost(SceneEditorServices& sceneServices)
        : m_SceneServices(sceneServices)
        , m_Surface(m_Capabilities, m_SceneServices)
        , m_Dispatcher(m_Capabilities, m_Surface)
    {
    }

    AutomationHost::~AutomationHost()
    {
        Stop();
    }

    bool AutomationHost::WriteReadyFile(const std::optional<std::filesystem::path>& readyFilePath) const
    {
        if (!readyFilePath.has_value())
        {
            return true;
        }

        std::error_code errorCode;
        const std::filesystem::path parent = readyFilePath->parent_path();
        if (!parent.empty())
        {
            std::filesystem::create_directories(parent, errorCode);
        }

        std::ofstream stream(readyFilePath->string(), std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            ME_LOG(LogEditor, Error, "AutomationHost: failed to write ready-file '{}'.", readyFilePath->string());
            return false;
        }

        stream << "AUTOMATION_READY pipe=" << m_PipePathNarrow << "\n";
        stream.flush();
        if (!stream)
        {
            ME_LOG(LogEditor, Error, "AutomationHost: ready-file write failed '{}'.", readyFilePath->string());
            return false;
        }

        ME_LOG(LogEditor, Info, "AutomationHost: wrote ready-file '{}'.", readyFilePath->string());
        return true;
    }

    bool AutomationHost::Start(
        const std::string& pipeName,
        const std::optional<std::filesystem::path>& readyFilePath)
    {
        Stop();

        m_PipePathWide = AutomationProtocol::NormalizePipePath(pipeName);
        m_PipePathNarrow = AutomationProtocol::NarrowPipePath(m_PipePathWide);

        if (!m_PipeServer.Listen(m_PipePathWide))
        {
            ME_LOG(
                LogEditor,
                Error,
                "AutomationHost: Listen failed for pipe '{}' (Win32={}).",
                m_PipePathNarrow,
                static_cast<unsigned>(GetLastError()));
            return false;
        }

        ME_LOG(LogEditor, Info, "AutomationHost: listening on '{}'.", m_PipePathNarrow);

        if (!WriteReadyFile(readyFilePath))
        {
            m_PipeServer.Close();
            return false;
        }

        m_StopRequested = false;
        m_Running = true;
        m_IoThread = std::thread([this]() { IoThreadMain(); });
        return true;
    }

    void AutomationHost::Stop()
    {
        if (!m_Running.load() && !m_IoThread.joinable())
        {
            return;
        }

        m_StopRequested = true;
        m_PipeServer.Close();

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Pending != nullptr)
            {
                m_Pending->Cancelled = true;
                m_Pending->Ready = true;
                m_IoCv.notify_all();
            }
        }

        if (m_IoThread.joinable())
        {
            m_IoThread.join();
        }

        m_Running = false;
        ME_LOG(LogEditor, Info, "AutomationHost: stopped.");
    }

    void AutomationHost::IoThreadMain()
    {
        while (!m_StopRequested.load())
        {
            if (!m_PipeServer.WaitForClient())
            {
                break;
            }

            ME_LOG(LogEditor, Info, "AutomationHost: client connected.");

            while (!m_StopRequested.load())
            {
                std::string requestJson;
                if (!m_PipeServer.ReadFrame(requestJson))
                {
                    break;
                }

                PendingExchange exchange;
                exchange.RequestJson = std::move(requestJson);

                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    m_Pending = &exchange;
                    m_MainCv.notify_one();
                    m_IoCv.wait(lock, [&exchange]() { return exchange.Ready; });
                    m_Pending = nullptr;
                }

                if (exchange.Cancelled || m_StopRequested.load())
                {
                    break;
                }

                if (!m_PipeServer.WriteFrame(exchange.ResponseJson))
                {
                    break;
                }
            }

            m_PipeServer.DisconnectClient();
            ME_LOG(LogEditor, Info, "AutomationHost: client disconnected.");
        }

        m_Running = false;
    }

    void AutomationHost::Tick(IEditorContext& context)
    {
        PendingExchange* exchange = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            exchange = m_Pending;
            if (exchange == nullptr || exchange->Ready)
            {
                return;
            }
        }

        const AutomationResponse response = m_Dispatcher.Dispatch(context, exchange->RequestJson);
        const std::string responseJson = response.ToJson().dump();

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Pending == exchange)
            {
                exchange->ResponseJson = responseJson;
                exchange->Ready = true;
                m_IoCv.notify_all();
            }
        }
    }
}
