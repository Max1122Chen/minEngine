#include "Runtime/Core/Profiling/Profile.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Profiling/ProfileClock.h"
#include "Runtime/Core/Profiling/ProfileNameRegistry.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace minEngine::Profile
{
    namespace
    {
        struct ProfileThreadBuffer
        {
            std::vector<ProfileEvent> Events;
            ProfileThreadId ThreadId = 0;
            uint64_t OsThreadId = 0;
            uint64_t Epoch = 0;
            uint8_t ScopeDepth = 0;
            ProfilePhaseId CurrentPhase = kInvalidPhaseId;
            ProfileFrameIndex CurrentFrame = kInvalidFrameIndex;
            uint64_t LocalDropped = 0;
            uint64_t LocalRecorded = 0;
            uint32_t LocalMaxDepth = 0;

            void ClearEvents()
            {
                Events.clear();
            }
        };

        struct ProfileSystemState
        {
            std::atomic<bool> Initialized{false};
            std::atomic<bool> Enabled{false};
            std::atomic<bool> Recording{false};
            bool AutoEngineSession = true;
            ProfileNameRegistry Names;
            std::unique_ptr<ProfileSession> Active;
            std::unique_ptr<ProfileSession> LastCompleted;
            std::atomic<ProfileSessionId> NextSessionId{1};
            std::atomic<ProfilePhaseId> NextPhaseId{1};
            std::atomic<ProfileThreadId> NextThreadId{1};
            std::atomic<ProfileFrameIndex> NextFrameIndex{0};
            std::atomic<uint64_t> RecordedForCap{0};
            std::atomic<uint64_t> NoopHits{0};
            std::atomic<uint64_t> DroppedEvents{0};

            // Recording calls hold this shared for their whole duration; session transitions and
            // buffer collection hold it exclusively. Collection therefore always observes
            // "recording has stopped", while threads still recording into other buffers are never
            // blocked by the drain and never see a half-built session.
            std::shared_mutex SessionLock;
            // Session-owned records (threads, phases, frames, events, stats) are merged from
            // several recording threads, so they are written under this mutex while the session
            // lock is only held shared.
            std::mutex SessionDataMutex;
            // Guards the registry of per-thread buffers, which threads join and leave while
            // recording.
            std::mutex RegistryMutex;
            std::vector<std::shared_ptr<ProfileThreadBuffer>> Buffers;
            std::atomic<uint64_t> Epoch{1};
        };

        ProfileSystemState& GetState()
        {
            // Intentionally never destroyed: a thread-exit drain or a late recording call can
            // still reach the state while the process tears down, and a destroyed singleton would
            // hand those callers released storage.
            static ProfileSystemState* s_State = new ProfileSystemState();
            return *s_State;
        }

        void NoteNoop(ProfileSystemState& state)
        {
            state.NoopHits.fetch_add(1, std::memory_order_relaxed);
        }

        // Unlocked fast path: one atomic load lets every instrumented call site bail out while the
        // profiler is off or no session is recording.
        bool IsRecordingRequested(ProfileSystemState& state)
        {
            return state.Initialized.load(std::memory_order_acquire) &&
                   state.Enabled.load(std::memory_order_relaxed) &&
                   state.Recording.load(std::memory_order_acquire);
        }

        // Requires SessionLock held (shared or exclusive).
        bool IsRecordingLocked(ProfileSystemState& state)
        {
            return state.Active != nullptr && state.Active->State == ProfileSessionState::Recording;
        }

        // Requires SessionDataMutex held, or SessionLock held exclusively.
        void FlushBufferIntoSession(ProfileSession& session, ProfileThreadBuffer& buffer)
        {
            if (!buffer.Events.empty())
            {
                session.Events.insert(session.Events.end(), buffer.Events.begin(), buffer.Events.end());
                buffer.ClearEvents();
            }

            session.Stats.EventsRecorded += buffer.LocalRecorded;
            session.Stats.EventsDropped += buffer.LocalDropped;
            if (buffer.LocalMaxDepth > session.Stats.MaxScopeDepth)
            {
                session.Stats.MaxScopeDepth = buffer.LocalMaxDepth;
            }

            buffer.LocalRecorded = 0;
            buffer.LocalDropped = 0;
            buffer.LocalMaxDepth = 0;
        }

        void EraseBufferRegistration(ProfileSystemState& state, const ProfileThreadBuffer* buffer)
        {
            std::lock_guard<std::mutex> registryLock(state.RegistryMutex);
            const auto end = state.Buffers.end();
            const auto it = std::find_if(
                state.Buffers.begin(),
                end,
                [buffer](const std::shared_ptr<ProfileThreadBuffer>& entry)
                {
                    return entry.get() == buffer;
                });
            if (it != end)
            {
                state.Buffers.erase(it);
            }
        }

        void ClearBufferRegistry(ProfileSystemState& state)
        {
            std::lock_guard<std::mutex> registryLock(state.RegistryMutex);
            state.Buffers.clear();
        }

        std::shared_ptr<ProfileThreadBuffer> CreateRegisteredBuffer(
            ProfileSystemState& state,
            uint64_t epoch)
        {
            auto buffer = std::make_shared<ProfileThreadBuffer>();
            buffer->Epoch = epoch;
            buffer->ThreadId = state.NextThreadId.fetch_add(1, std::memory_order_relaxed);
            buffer->OsThreadId = static_cast<uint64_t>(
                std::hash<std::thread::id>{}(std::this_thread::get_id()));

            {
                std::lock_guard<std::mutex> registryLock(state.RegistryMutex);
                state.Buffers.push_back(buffer);
            }

            {
                std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
                if (IsRecordingLocked(state) &&
                    buffer->Epoch == state.Epoch.load(std::memory_order_relaxed))
                {
                    ProfileThreadInfo info;
                    info.Id = buffer->ThreadId;
                    info.OsThreadId = buffer->OsThreadId;
                    if (state.Active->Desc.CaptureThreadNames)
                    {
                        std::ostringstream oss;
                        oss << "Thread-" << buffer->ThreadId;
                        info.Name = oss.str();
                    }
                    state.Active->Threads.push_back(std::move(info));
                }
            }

            return buffer;
        }

        // Runs on the owning thread when its thread-local buffer is released. The registry holds
        // strong references, so a buffer whose thread died without this drain running is still
        // alive and still collected; it is released later, when the registry is cleared.
        void DetachThreadBuffer(
            ProfileSystemState& state,
            const std::shared_ptr<ProfileThreadBuffer>& buffer)
        {
            if (buffer == nullptr)
            {
                return;
            }

            // Drain before the storage dies, so a thread that recorded events and then exited does
            // not lose them.
            std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
            EraseBufferRegistration(state, buffer.get());

            std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
            if (IsRecordingLocked(state) &&
                buffer->Epoch == state.Epoch.load(std::memory_order_relaxed))
            {
                FlushBufferIntoSession(*state.Active, *buffer);
            }
        }

        struct ThreadBufferSlot
        {
            std::shared_ptr<ProfileThreadBuffer> Buffer;

            ThreadBufferSlot() = default;
            ThreadBufferSlot(const ThreadBufferSlot&) = delete;
            ThreadBufferSlot& operator=(const ThreadBufferSlot&) = delete;

            ~ThreadBufferSlot()
            {
                DetachThreadBuffer(GetState(), Buffer);
                Buffer.reset();
            }
        };

        ThreadBufferSlot& GetThreadSlot()
        {
            static thread_local ThreadBufferSlot s_Slot;
            return s_Slot;
        }

        // Requires SessionLock held: registering a buffer adds it to the current session.
        ProfileThreadBuffer& GetThreadBuffer(ProfileSystemState& state)
        {
            ThreadBufferSlot& slot = GetThreadSlot();
            const uint64_t epoch = state.Epoch.load(std::memory_order_acquire);
            if (slot.Buffer != nullptr && slot.Buffer->Epoch == epoch)
            {
                return *slot.Buffer;
            }

            if (slot.Buffer != nullptr)
            {
                // The session that owned this buffer is over: its events were collected when that
                // session stopped, and the registry belongs to the new session.
                EraseBufferRegistration(state, slot.Buffer.get());
                slot.Buffer.reset();
            }

            slot.Buffer = CreateRegisteredBuffer(state, epoch);
            return *slot.Buffer;
        }

        // Requires SessionLock held and IsRecordingLocked(state).
        //
        // Take by const-ref: MinGW/GCC may emit vmovdqa for by-value ProfileEvent (~32 bytes),
        // but Win64 only guarantees 16-byte stack alignment (segfault from dtor call paths).
        void PushEvent(
            ProfileSystemState& state,
            ProfileThreadBuffer& buffer,
            const ProfileEvent& event)
        {
            ProfileSession* session = state.Active.get();
            if (session == nullptr ||
                state.RecordedForCap.load(std::memory_order_relaxed) >= session->Desc.MaxEventCount)
            {
                ++buffer.LocalDropped;
                state.DroppedEvents.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            buffer.Events.push_back(event);
            ProfileEvent& stored = buffer.Events.back();
            stored.Thread = buffer.ThreadId;
            stored.Phase = buffer.CurrentPhase;
            stored.Frame = buffer.CurrentFrame;

            ++buffer.LocalRecorded;
            state.RecordedForCap.fetch_add(1, std::memory_order_relaxed);

            if (stored.Type == ProfileEventType::ScopeBegin && stored.Depth > buffer.LocalMaxDepth)
            {
                buffer.LocalMaxDepth = stored.Depth;
            }
        }

        // Requires SessionLock held exclusively: no thread may be recording while it runs.
        void FlushAllBuffers(ProfileSystemState& state)
        {
            if (!state.Active)
            {
                return;
            }

            std::lock_guard<std::mutex> registryLock(state.RegistryMutex);
            std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
            for (const std::shared_ptr<ProfileThreadBuffer>& buffer : state.Buffers)
            {
                if (buffer != nullptr)
                {
                    FlushBufferIntoSession(*state.Active, *buffer);
                }
            }
        }

        // Requires SessionLock held (shared or exclusive).
        void CloseOpenPhaseAndFrame(ProfileSystemState& state)
        {
            if (!state.Active)
            {
                return;
            }

            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            const ProfileTimestamp now = ProfileClock::Now();

            std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);

            if (tls.CurrentFrame != kInvalidFrameIndex)
            {
                for (ProfileFrameRecord& frame : state.Active->Frames)
                {
                    if (frame.Index == tls.CurrentFrame && frame.End == 0)
                    {
                        frame.End = now;
                        break;
                    }
                }
                tls.CurrentFrame = kInvalidFrameIndex;
            }

            if (tls.CurrentPhase != kInvalidPhaseId)
            {
                for (ProfilePhaseRecord& phase : state.Active->Phases)
                {
                    if (phase.Id == tls.CurrentPhase && phase.Open)
                    {
                        phase.End = now;
                        phase.Open = false;
                        break;
                    }
                }
                tls.CurrentPhase = kInvalidPhaseId;
            }
        }

        // Recording entry points below share one shape: an unlocked fast-path check, then the
        // session lock in shared mode, then one of these *Locked bodies. Keeping the bodies
        // separate is what makes phase/frame nesting callable without re-entering the lock.

        // Requires SessionLock shared and IsRecordingLocked(state).
        void BeginPhaseLocked(ProfileSystemState& state, const char* staticName)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            if (tls.CurrentPhase != kInvalidPhaseId)
            {
                ME_LOG(LogCore, Warn, "Profile::BeginPhase rejected: nested phases are not supported");
                return;
            }

            const ProfileNameId nameId = state.Names.GetOrRegister(staticName);
            const ProfilePhaseId phaseId = state.NextPhaseId.fetch_add(1, std::memory_order_relaxed);
            const ProfileTimestamp now = ProfileClock::Now();

            {
                std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
                ProfilePhaseRecord record;
                record.Id = phaseId;
                record.Name = nameId;
                record.Start = now;
                record.Open = true;
                state.Active->Phases.push_back(record);
            }

            tls.CurrentPhase = phaseId;
            tls.CurrentFrame = kInvalidFrameIndex;

            ProfileEvent event;
            event.Type = ProfileEventType::PhaseBegin;
            event.Name = nameId;
            event.Depth = 0;
            event.Timestamp = now;
            PushEvent(state, tls, event);
        }

        // Requires SessionLock shared and IsRecordingLocked(state).
        void EndFrameLocked(ProfileSystemState& state)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            if (tls.CurrentFrame == kInvalidFrameIndex)
            {
                ME_LOG(LogCore, Warn, "Profile::EndFrame with no active frame");
                return;
            }

            const ProfileTimestamp now = ProfileClock::Now();
            {
                std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
                for (ProfileFrameRecord& frame : state.Active->Frames)
                {
                    if (frame.Index == tls.CurrentFrame && frame.End == 0)
                    {
                        frame.End = now;
                        break;
                    }
                }
            }

            ProfileEvent event;
            event.Type = ProfileEventType::FrameEnd;
            event.Name = state.Names.GetOrRegister("Frame");
            event.Depth = 0;
            event.Timestamp = now;
            PushEvent(state, tls, event);

            tls.CurrentFrame = kInvalidFrameIndex;

            std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
            FlushBufferIntoSession(*state.Active, tls);
        }

        // Requires SessionLock shared and IsRecordingLocked(state).
        void EndPhaseLocked(ProfileSystemState& state)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            if (tls.CurrentPhase == kInvalidPhaseId)
            {
                ME_LOG(LogCore, Warn, "Profile::EndPhase with no active phase");
                return;
            }

            if (tls.CurrentFrame != kInvalidFrameIndex)
            {
                EndFrameLocked(state);
            }

            const ProfileTimestamp now = ProfileClock::Now();
            ProfileNameId phaseName = kInvalidNameId;
            {
                std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
                for (ProfilePhaseRecord& phase : state.Active->Phases)
                {
                    if (phase.Id == tls.CurrentPhase && phase.Open)
                    {
                        phase.End = now;
                        phase.Open = false;
                        phaseName = phase.Name;
                        break;
                    }
                }
            }

            ProfileEvent event;
            event.Type = ProfileEventType::PhaseEnd;
            event.Name = phaseName;
            event.Depth = 0;
            event.Timestamp = now;
            PushEvent(state, tls, event);

            tls.CurrentPhase = kInvalidPhaseId;

            std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
            FlushBufferIntoSession(*state.Active, tls);
        }

        // Requires SessionLock shared and IsRecordingLocked(state).
        void BeginFrameLocked(ProfileSystemState& state)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            if (tls.CurrentPhase == kInvalidPhaseId)
            {
                ME_LOG(LogCore, Warn, "Profile::BeginFrame rejected: no active phase");
                return;
            }
            if (tls.CurrentFrame != kInvalidFrameIndex)
            {
                ME_LOG(LogCore, Warn, "Profile::BeginFrame rejected: frame already open");
                return;
            }

            const ProfileTimestamp now = ProfileClock::Now();
            ProfileFrameRecord frame;
            frame.Index = state.NextFrameIndex.fetch_add(1, std::memory_order_relaxed);
            frame.Phase = tls.CurrentPhase;
            frame.Start = now;

            {
                std::lock_guard<std::mutex> dataLock(state.SessionDataMutex);
                state.Active->Frames.push_back(frame);
            }
            tls.CurrentFrame = frame.Index;

            ProfileEvent event;
            event.Type = ProfileEventType::FrameBegin;
            event.Name = state.Names.GetOrRegister("Frame");
            event.Depth = 0;
            event.Timestamp = now;
            PushEvent(state, tls, event);
        }

        // Requires SessionLock shared and IsRecordingLocked(state).
        void BeginScopeLocked(ProfileSystemState& state, ProfileNameId name)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            const uint8_t depth = tls.ScopeDepth;
            if (tls.ScopeDepth < 255)
            {
                ++tls.ScopeDepth;
            }

            ProfileEvent event;
            event.Type = ProfileEventType::ScopeBegin;
            event.Name = name;
            event.Depth = depth;
            event.Timestamp = ProfileClock::Now();
            PushEvent(state, tls, event);
        }

        // Requires SessionLock shared and IsRecordingLocked(state).
        void EndScopeLocked(ProfileSystemState& state, ProfileNameId name)
        {
            ProfileThreadBuffer& tls = GetThreadBuffer(state);
            if (tls.ScopeDepth > 0)
            {
                --tls.ScopeDepth;
            }

            ProfileEvent event;
            event.Type = ProfileEventType::ScopeEnd;
            event.Name = name;
            event.Depth = tls.ScopeDepth;
            event.Timestamp = ProfileClock::Now();
            PushEvent(state, tls, event);
        }

        void EscapeJsonString(std::ostream& out, const char* text)
        {
            if (text == nullptr)
            {
                return;
            }
            for (const char* p = text; *p != '\0'; ++p)
            {
                const char c = *p;
                switch (c)
                {
                case '\\':
                    out << "\\\\";
                    break;
                case '"':
                    out << "\\\"";
                    break;
                case '\n':
                    out << "\\n";
                    break;
                case '\r':
                    out << "\\r";
                    break;
                case '\t':
                    out << "\\t";
                    break;
                default:
                    out << c;
                    break;
                }
            }
        }
    }

    double ProfileNameAggregate::AverageInclusiveMs() const
    {
        if (CallCount == 0)
        {
            return 0.0;
        }
        return ProfileClock::ToMilliseconds(TotalInclusive / CallCount);
    }

    void Initialize()
    {
        ProfileSystemState& state = GetState();
        if (state.Initialized.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        state.Enabled.store(false, std::memory_order_relaxed);
        state.Recording.store(false, std::memory_order_relaxed);
        state.NoopHits.store(0, std::memory_order_relaxed);
        state.DroppedEvents.store(0, std::memory_order_relaxed);
    }

    void Shutdown()
    {
        ProfileSystemState& state = GetState();
        if (!state.Initialized.load(std::memory_order_acquire))
        {
            return;
        }

        if (IsSessionActive())
        {
            StopSession();
        }

        std::unique_lock<std::shared_mutex> sessionLock(state.SessionLock);

        state.Recording.store(false, std::memory_order_release);
        state.Enabled.store(false, std::memory_order_relaxed);

        // A new epoch leaves buffers still held by live threads stale; the next recording call on
        // those threads replaces them instead of writing into a released session.
        state.Epoch.fetch_add(1, std::memory_order_acq_rel);
        ClearBufferRegistry(state);

        // Only the calling thread may release its own buffer.
        GetThreadSlot().Buffer.reset();

        state.Active.reset();
        state.LastCompleted.reset();
        state.NextSessionId.store(1, std::memory_order_relaxed);
        state.NextPhaseId.store(1, std::memory_order_relaxed);
        state.NextThreadId.store(1, std::memory_order_relaxed);
        state.NextFrameIndex.store(0, std::memory_order_relaxed);
        state.RecordedForCap.store(0, std::memory_order_relaxed);
        state.Initialized.store(false, std::memory_order_release);
    }

    void SetEnabled(bool enabled)
    {
        GetState().Enabled.store(enabled, std::memory_order_relaxed);
    }

    bool IsEnabled()
    {
#if !ME_ENABLE_PROFILER
        return false;
#else
        return GetState().Enabled.load(std::memory_order_relaxed);
#endif
    }

    bool IsCompileEnabled()
    {
#if ME_ENABLE_PROFILER
        return true;
#else
        return false;
#endif
    }

    void SetAutoEngineSessionEnabled(bool enabled)
    {
        GetState().AutoEngineSession = enabled;
    }

    bool IsAutoEngineSessionEnabled()
    {
        return GetState().AutoEngineSession;
    }

    ProfileSessionId StartSession(const ProfileSessionDesc& desc)
    {
        ProfileSystemState& state = GetState();
        Initialize();

        std::unique_lock<std::shared_mutex> sessionLock(state.SessionLock);

        if (IsRecordingLocked(state))
        {
            ME_LOG(LogCore, Warn, "Profile::StartSession rejected: session already active");
            return 0;
        }

        auto session = std::make_unique<ProfileSession>();
        session->Id = state.NextSessionId.fetch_add(1, std::memory_order_relaxed);
        session->State = ProfileSessionState::Recording;
        session->Desc = desc;
        session->Label = desc.Label != nullptr ? desc.Label : "default";
        session->SessionStart = ProfileClock::Now();
        state.NextFrameIndex.store(0, std::memory_order_relaxed);
        state.RecordedForCap.store(0, std::memory_order_relaxed);
        state.DroppedEvents.store(0, std::memory_order_relaxed);

        state.Active = std::move(session);
        // Every session owns a fresh buffer generation, so a thread that recorded in an earlier
        // session re-registers here instead of appending to a stale buffer.
        state.Epoch.fetch_add(1, std::memory_order_acq_rel);
        ClearBufferRegistry(state);

        state.Enabled.store(true, std::memory_order_relaxed);
        state.Recording.store(true, std::memory_order_release);

        // Register the starting thread up front: a session always reports the thread that owns it,
        // even before that thread records anything.
        (void)GetThreadBuffer(state);

        return state.Active->Id;
    }

    void StopSession()
    {
        ProfileSystemState& state = GetState();

        std::unique_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            return;
        }

        // Closed before the drain: a recorder that already passed the fast path re-checks the
        // session under the lock and turns into a noop instead of feeding a retired session.
        state.Recording.store(false, std::memory_order_release);

        CloseOpenPhaseAndFrame(state);
        FlushAllBuffers(state);

        state.Active->SessionEnd = ProfileClock::Now();
        state.Active->State = ProfileSessionState::Analyzing;
        AnalyzeSession(*state.Active);
        state.Active->State = ProfileSessionState::Completed;
        state.Active->Stats.NoopHits = state.NoopHits.load(std::memory_order_relaxed);

        state.LastCompleted = std::move(state.Active);
        state.Active.reset();
    }

    bool IsSessionActive()
    {
        return GetState().Recording.load(std::memory_order_acquire);
    }

    ProfileSessionId GetActiveSessionId()
    {
        const ProfileSystemState& state = GetState();
        return state.Active ? state.Active->Id : 0;
    }

    const ProfileSession* GetActiveSession()
    {
        return GetState().Active.get();
    }

    const ProfileSession* GetLastCompletedSession()
    {
        return GetState().LastCompleted.get();
    }

    uint64_t GetDroppedEventCount()
    {
        return GetState().DroppedEvents.load(std::memory_order_relaxed);
    }

    uint64_t GetNoopHitCount()
    {
        return GetState().NoopHits.load(std::memory_order_relaxed);
    }

    void BeginPhase(const char* staticName)
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        BeginPhaseLocked(state, staticName);
    }

    void EndPhase()
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        EndPhaseLocked(state);
    }

    void BeginFrame()
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        BeginFrameLocked(state);
    }

    void EndFrame()
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        EndFrameLocked(state);
    }

    void Detail_BeginScope(ProfileNameId name)
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        BeginScopeLocked(state, name);
    }

    void Detail_EndScope(ProfileNameId name)
    {
        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            return;
        }

        EndScopeLocked(state, name);
    }

    void ProfileScope_Begin(ProfileScopeState& scopeState, const char* staticLiteralName)
    {
        scopeState.Name = kInvalidNameId;
        scopeState.Active = 0;

        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        scopeState.Name = state.Names.GetOrRegister(staticLiteralName);
        scopeState.Active = 1;
        BeginScopeLocked(state, scopeState.Name);
    }

    void ProfileScope_End(ProfileScopeState& scopeState)
    {
        if (scopeState.Active == 0)
        {
            return;
        }

        ProfileSystemState& state = GetState();
        if (IsRecordingRequested(state))
        {
            std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
            if (IsRecordingLocked(state))
            {
                EndScopeLocked(state, scopeState.Name);
            }
        }

        scopeState.Active = 0;
        scopeState.Name = kInvalidNameId;
    }

    void ProfilePhase_Begin(ProfilePhaseState& phaseState, const char* staticName)
    {
        phaseState.Active = 0;

        ProfileSystemState& state = GetState();
        if (!IsRecordingRequested(state))
        {
            NoteNoop(state);
            return;
        }

        std::shared_lock<std::shared_mutex> sessionLock(state.SessionLock);
        if (!IsRecordingLocked(state))
        {
            NoteNoop(state);
            return;
        }

        BeginPhaseLocked(state, staticName);
        phaseState.Active = 1;
    }

    void ProfilePhase_End(ProfilePhaseState& phaseState)
    {
        if (phaseState.Active == 0)
        {
            return;
        }
        EndPhase();
        phaseState.Active = 0;
    }

    bool AnalyzeSession(ProfileSession& session)
    {
        session.Spans.clear();
        session.StatsSummary = {};

        struct OpenScope
        {
            size_t SpanIndex = 0;
            ProfileNameId Name = kInvalidNameId;
        };

        std::unordered_map<ProfileThreadId, std::vector<OpenScope>> stacks;

        std::vector<ProfileEvent> events = session.Events;
        std::stable_sort(
            events.begin(),
            events.end(),
            [](const ProfileEvent& a, const ProfileEvent& b)
            {
                if (a.Timestamp != b.Timestamp)
                {
                    return a.Timestamp < b.Timestamp;
                }
                if (a.Thread != b.Thread)
                {
                    return a.Thread < b.Thread;
                }
                // End before Begin at same timestamp on unwind edge cases.
                return static_cast<uint8_t>(a.Type) > static_cast<uint8_t>(b.Type);
            });

        for (const ProfileEvent& event : events)
        {
            if (event.Type != ProfileEventType::ScopeBegin && event.Type != ProfileEventType::ScopeEnd)
            {
                continue;
            }

            auto& stack = stacks[event.Thread];
            if (event.Type == ProfileEventType::ScopeBegin)
            {
                ProfileSpan span;
                span.Name = event.Name;
                span.Thread = event.Thread;
                span.Phase = event.Phase;
                span.Frame = event.Frame;
                span.Start = event.Timestamp;
                span.Depth = event.Depth;
                span.ParentSpanIndex = stack.empty() ? -1 : static_cast<int32_t>(stack.back().SpanIndex);
                const size_t index = session.Spans.size();
                session.Spans.push_back(span);
                if (span.ParentSpanIndex >= 0)
                {
                    ++session.Spans[static_cast<size_t>(span.ParentSpanIndex)].ChildCount;
                }
                stack.push_back(OpenScope{index, event.Name});
            }
            else
            {
                if (stack.empty())
                {
                    ++session.Stats.UnbalancedScopes;
                    continue;
                }

                OpenScope open = stack.back();
                stack.pop_back();
                ProfileSpan& span = session.Spans[open.SpanIndex];
                span.End = event.Timestamp;
                span.Inclusive = span.End >= span.Start ? (span.End - span.Start) : 0;
            }
        }

        for (const auto& pair : stacks)
        {
            session.Stats.UnbalancedScopes += static_cast<uint32_t>(pair.second.size());
            for (const OpenScope& open : pair.second)
            {
                ProfileSpan& span = session.Spans[open.SpanIndex];
                if (span.End == 0)
                {
                    span.End = session.SessionEnd != 0 ? session.SessionEnd : ProfileClock::Now();
                    span.Inclusive = span.End >= span.Start ? (span.End - span.Start) : 0;
                }
            }
        }

        // Exclusive = Inclusive - sum(direct children Inclusive)
        for (ProfileSpan& span : session.Spans)
        {
            span.Exclusive = span.Inclusive;
        }
        for (const ProfileSpan& span : session.Spans)
        {
            if (span.ParentSpanIndex >= 0)
            {
                ProfileSpan& parent = session.Spans[static_cast<size_t>(span.ParentSpanIndex)];
                if (parent.Exclusive >= span.Inclusive)
                {
                    parent.Exclusive -= span.Inclusive;
                }
                else
                {
                    parent.Exclusive = 0;
                }
            }
        }

        std::unordered_map<ProfileNameId, size_t> aggregateIndex;
        for (const ProfileSpan& span : session.Spans)
        {
            if (span.End == 0)
            {
                continue;
            }
            auto it = aggregateIndex.find(span.Name);
            if (it == aggregateIndex.end())
            {
                ProfileNameAggregate agg;
                agg.Name = span.Name;
                agg.CallCount = 1;
                agg.TotalInclusive = span.Inclusive;
                agg.TotalExclusive = span.Exclusive;
                agg.MinInclusive = span.Inclusive;
                agg.MaxInclusive = span.Inclusive;
                aggregateIndex.emplace(span.Name, session.StatsSummary.ByName.size());
                session.StatsSummary.ByName.push_back(agg);
            }
            else
            {
                ProfileNameAggregate& agg = session.StatsSummary.ByName[it->second];
                ++agg.CallCount;
                agg.TotalInclusive += span.Inclusive;
                agg.TotalExclusive += span.Exclusive;
                agg.MinInclusive = (std::min)(agg.MinInclusive, span.Inclusive);
                agg.MaxInclusive = (std::max)(agg.MaxInclusive, span.Inclusive);
            }
        }

        session.StatsSummary.SessionDuration =
            session.SessionEnd >= session.SessionStart ? (session.SessionEnd - session.SessionStart) : 0;
        session.StatsSummary.FrameCount = static_cast<uint32_t>(session.Frames.size());

        ProfileTimestamp totalFrame = 0;
        ProfileTimestamp minFrame = 0;
        ProfileTimestamp maxFrame = 0;
        bool hasFrame = false;
        for (const ProfileFrameRecord& frame : session.Frames)
        {
            if (frame.End == 0 || frame.End < frame.Start)
            {
                continue;
            }
            const ProfileTimestamp dur = frame.End - frame.Start;
            totalFrame += dur;
            if (!hasFrame)
            {
                minFrame = dur;
                maxFrame = dur;
                hasFrame = true;
            }
            else
            {
                minFrame = (std::min)(minFrame, dur);
                maxFrame = (std::max)(maxFrame, dur);
            }
        }
        if (hasFrame && session.StatsSummary.FrameCount > 0)
        {
            session.StatsSummary.AvgFrameDuration = totalFrame / session.StatsSummary.FrameCount;
            session.StatsSummary.MinFrameDuration = minFrame;
            session.StatsSummary.MaxFrameDuration = maxFrame;
        }

        return true;
    }

    const ProfileStatsSummary* GetStatsSummary(const ProfileSession& session)
    {
        return &session.StatsSummary;
    }

    const ProfileNameAggregate* FindNameAggregate(const ProfileSession& session, ProfileNameId name)
    {
        for (const ProfileNameAggregate& agg : session.StatsSummary.ByName)
        {
            if (agg.Name == name)
            {
                return &agg;
            }
        }
        return nullptr;
    }

    const ProfileNameAggregate* FindNameAggregate(const ProfileSession& session, const char* staticName)
    {
        const ProfileNameId id = GetState().Names.GetOrRegister(staticName);
        return FindNameAggregate(session, id);
    }

    const ProfilePhaseRecord* FindPhase(const ProfileSession& session, const char* staticName)
    {
        const ProfileNameId id = GetState().Names.GetOrRegister(staticName);
        for (const ProfilePhaseRecord& phase : session.Phases)
        {
            if (phase.Name == id)
            {
                return &phase;
            }
        }
        return nullptr;
    }

    const ProfileFrameRecord* FindFrame(const ProfileSession& session, ProfileFrameIndex index)
    {
        for (const ProfileFrameRecord& frame : session.Frames)
        {
            if (frame.Index == index)
            {
                return &frame;
            }
        }
        return nullptr;
    }

    size_t GetSpanCount(const ProfileSession& session)
    {
        return session.Spans.size();
    }

    const ProfileSpan* GetSpan(const ProfileSession& session, size_t index)
    {
        if (index >= session.Spans.size())
        {
            return nullptr;
        }
        return &session.Spans[index];
    }

    const char* GetNameText(ProfileNameId id)
    {
        return GetState().Names.GetText(id);
    }

    ProfileNameId RegisterName(const char* staticLiteral)
    {
        Initialize();
        return GetState().Names.GetOrRegister(staticLiteral);
    }

    bool ExportChromeTrace(
        const ProfileSession& session,
        const std::filesystem::path& path,
        const ProfileExportDesc& desc)
    {
        if (session.State != ProfileSessionState::Completed && session.Spans.empty())
        {
            ME_LOG(LogCore, Warn, "Profile::ExportChromeTrace requires analyzed session");
            return false;
        }

        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            ME_LOG(LogCore, Error, "Profile::ExportChromeTrace failed to open '{}'", path.string());
            return false;
        }

        const char* nl = desc.PrettyPrint ? "\n" : "";
        const char* indent = desc.PrettyPrint ? "  " : "";

        out << "{" << nl;
        out << indent << "\"displayTimeUnit\":\"ms\"," << nl;
        out << indent << "\"traceEvents\":[" << nl;

        bool first = true;
        auto writeComma = [&]()
        {
            if (!first)
            {
                out << "," << nl;
            }
            first = false;
        };

        for (const ProfileThreadInfo& thread : session.Threads)
        {
            writeComma();
            out << indent << indent << "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":1,\"tid\":"
                << thread.Id << ",\"args\":{\"name\":\"";
            EscapeJsonString(out, thread.Name.c_str());
            out << "\"}}";
        }

        for (const ProfilePhaseRecord& phase : session.Phases)
        {
            writeComma();
            out << indent << indent << "{\"name\":\"Phase:";
            EscapeJsonString(out, GetNameText(phase.Name));
            out << "\",\"ph\":\"i\",\"pid\":1,\"tid\":1,\"s\":\"g\",\"ts\":"
                << ProfileClock::ToMicroseconds(phase.Start) << "}";
        }

        for (const ProfileFrameRecord& frame : session.Frames)
        {
            writeComma();
            out << indent << indent << "{\"name\":\"Frame " << frame.Index
                << "\",\"ph\":\"i\",\"pid\":1,\"tid\":1,\"s\":\"g\",\"ts\":"
                << ProfileClock::ToMicroseconds(frame.Start) << "}";
        }

        for (const ProfileSpan& span : session.Spans)
        {
            if (span.End == 0)
            {
                continue;
            }
            writeComma();
            out << indent << indent << "{\"name\":\"";
            EscapeJsonString(out, GetNameText(span.Name));
            out << "\",\"ph\":\"X\",\"pid\":1,\"tid\":" << span.Thread << ",\"ts\":"
                << ProfileClock::ToMicroseconds(span.Start) << ",\"dur\":"
                << ProfileClock::ToMicroseconds(span.Inclusive) << "}";
        }

        out << nl << indent << "]";

        if (desc.IncludeStatsSummaryAsMetadata)
        {
            out << "," << nl << indent << "\"minEngine\":{" << nl;
            out << indent << indent << "\"schemaVersion\":" << session.SchemaVersion << "," << nl;
            out << indent << indent << "\"sessionId\":" << session.Id << "," << nl;
            out << indent << indent << "\"label\":\"";
            EscapeJsonString(out, session.Label.c_str());
            out << "\"," << nl;
            out << indent << indent << "\"frameCount\":" << session.StatsSummary.FrameCount << "," << nl;
            out << indent << indent << "\"spanCount\":" << session.Spans.size() << "," << nl;
            out << indent << indent << "\"eventsDropped\":" << session.Stats.EventsDropped << nl;
            out << indent << "}" << nl;
        }
        else
        {
            out << nl;
        }

        out << "}" << nl;
        return static_cast<bool>(out);
    }

    bool ExportStatsText(const ProfileSession& session, const std::filesystem::path& path)
    {
        std::ofstream out(path);
        if (!out)
        {
            return false;
        }

        out << "Profile session " << session.Id << " (" << session.Label << ")\n";
        out << "Duration ms: " << ProfileClock::ToMilliseconds(session.StatsSummary.SessionDuration) << "\n";
        out << "Frames: " << session.StatsSummary.FrameCount << "\n";
        out << "Name,Count,TotalInclusiveMs,AvgInclusiveMs,MinMs,MaxMs,TotalExclusiveMs\n";
        for (const ProfileNameAggregate& agg : session.StatsSummary.ByName)
        {
            out << GetNameText(agg.Name) << "," << agg.CallCount << ","
                << ProfileClock::ToMilliseconds(agg.TotalInclusive) << "," << agg.AverageInclusiveMs()
                << "," << ProfileClock::ToMilliseconds(agg.MinInclusive) << ","
                << ProfileClock::ToMilliseconds(agg.MaxInclusive) << ","
                << ProfileClock::ToMilliseconds(agg.TotalExclusive) << "\n";
        }
        return static_cast<bool>(out);
    }
}
