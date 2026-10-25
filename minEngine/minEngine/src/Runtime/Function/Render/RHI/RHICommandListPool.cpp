#include "RHICommandListPool.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Profiling/Profile.h"

namespace minEngine
{
    RHICommandListPool* RHICommandListPool::s_Instance = nullptr;

    bool RHICommandListPool::HasInstance()
    {
        return s_Instance != nullptr;
    }

    RHICommandListPool& RHICommandListPool::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "RHICommandListPool is not initialized");
        return *s_Instance;
    }

    void RHICommandListPool::Initialize(RHI* rhi)
    {
        ME_ASSERT(s_Instance == nullptr, "RHICommandListPool already initialized");
        ME_ASSERT(rhi != nullptr, "RHICommandListPool requires RHI");
        s_Instance = this;
        m_RHI = rhi;
        m_Initialized = true;
        m_ShuttingDown = false;

        m_Owners.clear();
        m_Free.clear();
        m_Finished.clear();
        m_Owners.reserve(kMaxInFlightCommandLists);
        for (size_t i = 0; i < kMaxInFlightCommandLists; ++i)
        {
            auto list = std::make_unique<RHICommandList>(rhi);
            list->SetImmediateOverride(false);
            m_Free.push_back(list.get());
            m_Owners.push_back(std::move(list));
        }
    }

    void RHICommandListPool::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        RequestShutdown();
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Free.clear();
            m_Finished.clear();
            m_Owners.clear();
        }
        m_Initialized = false;
        m_RHI = nullptr;
        s_Instance = nullptr;
    }

    void RHICommandListPool::RequestShutdown()
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_ShuttingDown = true;
        }
        m_FreeCv.notify_all();
        m_FinishedCv.notify_all();
    }

    RHICommandList* RHICommandListPool::Acquire()
    {
        ME_ASSERT(m_Initialized, "RHICommandListPool::Acquire before Initialize");
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (m_Free.empty() && !m_ShuttingDown)
        {
            ME_PROFILE_SCOPE("Frame.Render.WaitRHI");
            ME_LOG(LogRender, Warn, "RHICommandListPool full ({}); blocking Render.", kMaxInFlightCommandLists);
            m_FreeCv.wait(lock);
        }
        if (m_ShuttingDown || m_Free.empty())
        {
            return nullptr;
        }
        RHICommandList* list = m_Free.front();
        m_Free.pop_front();
        list->Reset();
        list->SetImmediateOverride(false);
        return list;
    }

    void RHICommandListPool::SubmitFinished(RHICommandList* list)
    {
        if (list == nullptr)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Finished.push_back(list);
        }
        m_FinishedCv.notify_one();
    }

    RHICommandList* RHICommandListPool::WaitTakeFinished()
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        while (m_Finished.empty() && !m_ShuttingDown)
        {
            m_FinishedCv.wait(lock);
        }
        if (m_Finished.empty())
        {
            return nullptr;
        }
        RHICommandList* list = m_Finished.front();
        m_Finished.pop_front();
        return list;
    }

    RHICommandList* RHICommandListPool::TryTakeFinished()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Finished.empty())
        {
            return nullptr;
        }
        RHICommandList* list = m_Finished.front();
        m_Finished.pop_front();
        return list;
    }

    void RHICommandListPool::Release(RHICommandList* list)
    {
        if (list == nullptr)
        {
            return;
        }
        list->Reset();
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Free.push_back(list);
        }
        m_FreeCv.notify_one();
    }
}
