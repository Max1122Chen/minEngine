#include "Runtime/Function/Pointer/PointerModeRequest.h"
#include "Runtime/Function/Pointer/PointerPolicyService.h"

namespace minEngine
{
    PointerModeRequest::PointerModeRequest(PointerPolicyService* service, uint64_t slotId)
        : m_Service(service)
        , m_SlotId(slotId)
    {
    }

    PointerModeRequest::PointerModeRequest(PointerModeRequest&& other) noexcept
        : m_Service(other.m_Service)
        , m_SlotId(other.m_SlotId)
    {
        other.m_Service = nullptr;
        other.m_SlotId = 0;
    }

    PointerModeRequest& PointerModeRequest::operator=(PointerModeRequest&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Service = other.m_Service;
            m_SlotId = other.m_SlotId;
            other.m_Service = nullptr;
            other.m_SlotId = 0;
        }
        return *this;
    }

    PointerModeRequest::~PointerModeRequest()
    {
        Release();
    }

    bool PointerModeRequest::IsValid() const
    {
        if (m_Service == nullptr || m_SlotId == 0)
        {
            return false;
        }
        return m_Service->FindSlot(m_SlotId) != nullptr
            && m_Service->FindSlot(m_SlotId)->RefCount > 0;
    }

    void PointerModeRequest::Release()
    {
        if (m_Service != nullptr && m_SlotId != 0)
        {
            m_Service->ReleaseSlot(m_SlotId);
        }
        m_Service = nullptr;
        m_SlotId = 0;
    }
}
