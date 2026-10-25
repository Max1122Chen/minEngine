#pragma once

#include "Runtime/Core/EngineAPI.h"

#include <cstdint>

namespace minEngine
{
    class PointerPolicyService;

    /** Move-only RAII handle; destructor releases one ref on the stacked request. */
    class MINENGINE_API PointerModeRequest
    {
    public:
        PointerModeRequest() = default;
        PointerModeRequest(PointerModeRequest&& other) noexcept;
        PointerModeRequest& operator=(PointerModeRequest&& other) noexcept;
        ~PointerModeRequest();

        PointerModeRequest(const PointerModeRequest&) = delete;
        PointerModeRequest& operator=(const PointerModeRequest&) = delete;

        void Release();
        bool IsValid() const;
        uint64_t GetSlotId() const { return m_SlotId; }

    private:
        friend class PointerPolicyService;
        PointerModeRequest(PointerPolicyService* service, uint64_t slotId);

        PointerPolicyService* m_Service = nullptr;
        uint64_t m_SlotId = 0;
    };
}
