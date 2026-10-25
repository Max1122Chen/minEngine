#pragma once

#include "Runtime/Core/EngineAPI.h"
#include "Runtime/Function/Pointer/IPointerModeSink.h"
#include "Runtime/Function/Pointer/PointerModeRequest.h"
#include "Runtime/Function/Pointer/PointerPolicyTypes.h"

#include <vector>

namespace minEngine
{
    class MINENGINE_API PointerPolicyService
    {
    public:
        PointerPolicyService() = default;
        ~PointerPolicyService();

        PointerPolicyService(const PointerPolicyService&) = delete;
        PointerPolicyService& operator=(const PointerPolicyService&) = delete;

        static bool HasInstance();
        static PointerPolicyService& Get();
        static void SetInstance(PointerPolicyService* instance);

        void Initialize();
        void Shutdown();

        /** Tests / Engine wire the sink before Request/BeginFrame. */
        void SetSink(IPointerModeSink* sink);

        PointerModeRequest Request(EPointerTier tier, EPointerMode mode, const PointerClipSpec& clip = {});
        /** Refresh clip on an existing request without changing tier/mode or refcount. */
        void UpdateClip(PointerModeRequest& handle, const PointerClipSpec& clip);

        EPointerMode GetResolvedMode() const;
        PointerResolvedState GetResolvedState() const;
        bool IsPointerVisible() const;
        bool IsPointerCaptured() const;

        PointerAppliedConstraint GetAppliedConstraint() const;
        const PointerPositionSample& GetPosition() const { return m_Position; }

        void OnWindowFocusLost();
        void OnWindowFocusGained();

        /** Watchdog + resolve/apply + (later) position sample. Call once per frame before UI pointer poll. */
        void BeginFrame();

        /** Drop every stacked request and apply Free (Stop / Shutdown). */
        void ClearAllRequestsAndApplyFree();

        PointerPolicySettings& GetSettings() { return m_Settings; }
        const PointerPolicySettings& GetSettings() const { return m_Settings; }

        /**
         * True when the host should release PIE look capture this frame:
         * Shift+F1 (primary), or LeftAlt when bKeepLeftAltAlias.
         */
        bool IsReleaseRequestedThisFrame() const;

        uint32_t GetOutOfOrderReleaseWarnCount() const { return m_OutOfOrderReleaseWarnCount; }

    private:
        friend class PointerModeRequest;

        struct StackSlot
        {
            uint64_t Id = 0;
            EPointerTier Tier = EPointerTier::Default;
            EPointerMode Mode = EPointerMode::Free;
            PointerClipSpec Clip{};
            int RefCount = 0;
            uint64_t Sequence = 0;
        };

        void ReleaseSlot(uint64_t slotId);
        bool TryResolve(EPointerMode& outMode, EPointerTier& outTier, PointerClipSpec& outClip, int& outDepth) const;
        void ResolveAndApply();
        void ApplyImmediateFree();
        void ApplyCommand(const PointerWindowCommand& command);
        StackSlot* FindSlot(uint64_t slotId);
        const StackSlot* FindSlot(uint64_t slotId) const;

        static PointerPolicyService* s_Instance;

        IPointerModeSink* m_Sink = nullptr;
        std::vector<StackSlot> m_Stack;
        uint64_t m_NextSlotId = 1;
        uint64_t m_NextSequence = 1;
        bool bWindowActive = true;
        bool bSuspendedByFocus = false;
        bool bDirty = true;
        PointerWindowCommand m_LastApplied{};
        bool bHasLastApplied = false;
        PointerPositionSample m_Position{};
        PointerAppliedConstraint m_LastAppliedConstraint{};
        PointerPolicySettings m_Settings{};
        uint32_t m_OutOfOrderReleaseWarnCount = 0;
        bool bInitialized = false;
    };
}
