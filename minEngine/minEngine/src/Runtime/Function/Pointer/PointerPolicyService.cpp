#include "Runtime/Function/Pointer/PointerPolicyService.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Input/InputSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/UI/UISystem.h"

#include <algorithm>

namespace minEngine
{
    PointerPolicyService* PointerPolicyService::s_Instance = nullptr;

    PointerPolicyService::~PointerPolicyService()
    {
        if (s_Instance == this)
        {
            s_Instance = nullptr;
        }
    }

    bool PointerPolicyService::HasInstance()
    {
        return s_Instance != nullptr;
    }

    PointerPolicyService& PointerPolicyService::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "PointerPolicyService has no instance");
        return *s_Instance;
    }

    void PointerPolicyService::SetInstance(PointerPolicyService* instance)
    {
        s_Instance = instance;
    }

    void PointerPolicyService::Initialize()
    {
        bInitialized = true;
        bWindowActive = true;
        bSuspendedByFocus = false;
        bDirty = true;
        ResolveAndApply();
    }

    void PointerPolicyService::Shutdown()
    {
        ClearAllRequestsAndApplyFree();
        m_Sink = nullptr;
        bInitialized = false;
    }

    void PointerPolicyService::SetSink(IPointerModeSink* sink)
    {
        m_Sink = sink;
        bDirty = true;
    }

    PointerModeRequest PointerPolicyService::Request(
        EPointerTier tier,
        EPointerMode mode,
        const PointerClipSpec& clip)
    {
        for (StackSlot& slot : m_Stack)
        {
            if (slot.RefCount > 0 && slot.Tier == tier && slot.Mode == mode
                && PointerClipSpecsEqual(slot.Clip, clip))
            {
                ++slot.RefCount;
                bDirty = true;
                ResolveAndApply();
                return PointerModeRequest(this, slot.Id);
            }
        }

        StackSlot slot;
        slot.Id = m_NextSlotId++;
        slot.Tier = tier;
        slot.Mode = mode;
        slot.Clip = clip;
        slot.RefCount = 1;
        slot.Sequence = m_NextSequence++;
        m_Stack.push_back(slot);
        bDirty = true;
        ResolveAndApply();
        return PointerModeRequest(this, slot.Id);
    }

    void PointerPolicyService::UpdateClip(PointerModeRequest& handle, const PointerClipSpec& clip)
    {
        if (!handle.IsValid())
        {
            return;
        }
        StackSlot* slot = FindSlot(handle.GetSlotId());
        if (slot == nullptr || slot->RefCount <= 0)
        {
            return;
        }
        if (PointerClipSpecsEqual(slot->Clip, clip))
        {
            return;
        }
        slot->Clip = clip;
        bDirty = true;
        ResolveAndApply();
    }

    void PointerPolicyService::ReleaseSlot(uint64_t slotId)
    {
        StackSlot* slot = FindSlot(slotId);
        if (slot == nullptr || slot->RefCount <= 0)
        {
            // Idempotent: Stop/Shutdown may ClearAll before RAII handles destruct.
            return;
        }

        --slot->RefCount;
        if (slot->RefCount <= 0)
        {
            const auto it = std::find_if(
                m_Stack.begin(),
                m_Stack.end(),
                [slotId](const StackSlot& entry) { return entry.Id == slotId; });
            if (it != m_Stack.end())
            {
                m_Stack.erase(it);
            }
        }
        bDirty = true;
        ResolveAndApply();
    }

    bool PointerPolicyService::TryResolve(
        EPointerMode& outMode,
        EPointerTier& outTier,
        PointerClipSpec& outClip,
        int& outDepth) const
    {
        outDepth = 0;
        const StackSlot* best = nullptr;
        for (const StackSlot& slot : m_Stack)
        {
            if (slot.RefCount <= 0)
            {
                continue;
            }
            ++outDepth;
            if (best == nullptr)
            {
                best = &slot;
                continue;
            }
            if (static_cast<int>(slot.Tier) > static_cast<int>(best->Tier))
            {
                best = &slot;
            }
            else if (slot.Tier == best->Tier && slot.Sequence > best->Sequence)
            {
                best = &slot;
            }
        }

        if (best == nullptr)
        {
            outMode = EPointerMode::Free;
            outTier = EPointerTier::Default;
            outClip = {};
            return false;
        }

        outMode = best->Mode;
        outTier = best->Tier;
        outClip = best->Clip;
        return true;
    }

    EPointerMode PointerPolicyService::GetResolvedMode() const
    {
        return GetResolvedState().Mode;
    }

    PointerResolvedState PointerPolicyService::GetResolvedState() const
    {
        PointerResolvedState state;
        EPointerMode mode = EPointerMode::Free;
        EPointerTier tier = EPointerTier::Default;
        PointerClipSpec clip{};
        int depth = 0;
        TryResolve(mode, tier, clip, depth);
        state.StackDepth = depth;
        state.bSuspendedByFocus = bSuspendedByFocus;
        if (bSuspendedByFocus)
        {
            state.Mode = EPointerMode::Free;
            state.Tier = EPointerTier::Default;
            state.Clip = {};
            return state;
        }
        state.Mode = mode;
        state.Tier = tier;
        state.Clip = clip;
        return state;
    }

    bool PointerPolicyService::IsPointerVisible() const
    {
        const EPointerMode mode = GetResolvedMode();
        return mode == EPointerMode::Free || mode == EPointerMode::Confined;
    }

    bool PointerPolicyService::IsPointerCaptured() const
    {
        return GetResolvedMode() == EPointerMode::Captured;
    }

    PointerAppliedConstraint PointerPolicyService::GetAppliedConstraint() const
    {
        if (WindowSystem::HasInstance())
        {
            const WindowCursorConstraintState os = WindowSystem::Get().GetCursorConstraintState();
            PointerAppliedConstraint applied;
            applied.NativeCursorMode = os.NativeCursorMode;
            applied.bClipActive = os.bClipActive;
            applied.ClipRect = os.ClipRect;
            applied.bRawCaptureActive = os.bRawCaptureActive;
            applied.bWindowActive = os.bWindowActive;
            applied.bObservable = os.bObservable;
            return applied;
        }
        return m_LastAppliedConstraint;
    }

    void PointerPolicyService::OnWindowFocusLost()
    {
        bWindowActive = false;
        bSuspendedByFocus = true;
        ApplyImmediateFree();
        bDirty = false;
    }

    void PointerPolicyService::OnWindowFocusGained()
    {
        bWindowActive = true;
        bSuspendedByFocus = false;
        bDirty = true;
        ResolveAndApply();
    }

    bool PointerPolicyService::IsReleaseRequestedThisFrame() const
    {
        if (!InputSystem::HasInstance())
        {
            return false;
        }

        const bool bShiftHeld = InputSystem::KeyDown(InputKeys::Key_LeftShift)
            || InputSystem::KeyDown(InputKeys::Key_RightShift)
            || InputSystem::KeyDown(InputKeys::Key_Shift);
        if (bShiftHeld && InputSystem::KeyPressed(InputKeys::Key_F1))
        {
            return true;
        }

        if (m_Settings.bKeepLeftAltAlias && InputSystem::KeyPressed(InputKeys::Key_LeftAlt))
        {
            return true;
        }

        return false;
    }

    void PointerPolicyService::BeginFrame()
    {
        if (WindowSystem::HasInstance())
        {
            const WindowCursorConstraintState os = WindowSystem::Get().GetCursorConstraintState();
            if (!os.bWindowActive && !bSuspendedByFocus)
            {
                // Watchdog: platform inactive but we still think we own constraints.
                OnWindowFocusLost();
                ME_LOG(LogPlatform, Warn, "PointerPolicy watchdog: window inactive while policy was not suspended; forced Free.");
            }
            else if (os.bWindowActive && bSuspendedByFocus)
            {
                OnWindowFocusGained();
            }
        }

        if (bDirty || bSuspendedByFocus)
        {
            ResolveAndApply();
        }

        // Position sample once per frame (before UI PollPointer). ReferencePx uses last UI sample when available.
        ++m_Position.FrameStamp;
        m_Position.RelativeDelta = Vector2(0.0f, 0.0f);
        m_Position.bAbsoluteValid = false;
        m_Position.bReferenceValid = false;
        if (InputSystem::HasInstance())
        {
            m_Position.WindowPx = InputSystem::GetMousePosition();
            const EPointerMode mode = GetResolvedMode();
            m_Position.bAbsoluteValid = !bSuspendedByFocus && mode != EPointerMode::Captured;
            if (UISystem::HasInstance())
            {
                const ScreenUIPointerViewportContext& ctx = UISystem::Get().GetPointerViewportContext();
                if (ctx.bValid)
                {
                    m_Position.ViewportPointPx = m_Position.WindowPx - ctx.ImageMin;
                }
                const ScreenUIPointerViewportSample& uiSample = UISystem::Get().GetPointerPosition();
                if (uiSample.bValid)
                {
                    m_Position.ReferencePx = uiSample.ReferencePx;
                    m_Position.bReferenceValid = m_Position.bAbsoluteValid;
                }
            }
        }
    }

    void PointerPolicyService::ClearAllRequestsAndApplyFree()
    {
        m_Stack.clear();
        bSuspendedByFocus = false;
        bWindowActive = true;
        ApplyImmediateFree();
        bDirty = false;
    }

    void PointerPolicyService::ResolveAndApply()
    {
        if (bSuspendedByFocus || !bWindowActive)
        {
            ApplyImmediateFree();
            bDirty = false;
            return;
        }

        EPointerMode mode = EPointerMode::Free;
        EPointerTier tier = EPointerTier::Default;
        PointerClipSpec clip{};
        int depth = 0;
        TryResolve(mode, tier, clip, depth);
        (void)tier;
        (void)depth;

        const PointerWindowCommand command = MapModeToWindowCommand(mode, true, clip);
        ApplyCommand(command);
        bDirty = false;
    }

    void PointerPolicyService::ApplyImmediateFree()
    {
        PointerClipSpec empty;
        ApplyCommand(MapModeToWindowCommand(EPointerMode::Free, false, empty));
    }

    void PointerPolicyService::ApplyCommand(const PointerWindowCommand& command)
    {
        if (m_Sink != nullptr)
        {
            m_Sink->Apply(command);
        }

        m_LastApplied = command;
        bHasLastApplied = true;
        m_LastAppliedConstraint.NativeCursorMode = command.CursorMode;
        m_LastAppliedConstraint.bClipActive = command.bClip;
        m_LastAppliedConstraint.ClipRect = command.ClipRect;
        m_LastAppliedConstraint.bRawCaptureActive = command.bRelativeCapture;
        m_LastAppliedConstraint.bWindowActive = bWindowActive && !bSuspendedByFocus;
        m_LastAppliedConstraint.bObservable = false;
    }

    PointerPolicyService::StackSlot* PointerPolicyService::FindSlot(uint64_t slotId)
    {
        for (StackSlot& slot : m_Stack)
        {
            if (slot.Id == slotId)
            {
                return &slot;
            }
        }
        return nullptr;
    }

    const PointerPolicyService::StackSlot* PointerPolicyService::FindSlot(uint64_t slotId) const
    {
        for (const StackSlot& slot : m_Stack)
        {
            if (slot.Id == slotId)
            {
                return &slot;
            }
        }
        return nullptr;
    }
}
