#pragma once

#include "Runtime/Core/Profiling/ProfileTypes.h"

#include "EngineAPI.h"

#ifndef ME_ENABLE_PROFILER
#define ME_ENABLE_PROFILER 1
#endif

#define ME_PROFILE_CONCAT_INNER(a, b) a##b
#define ME_PROFILE_CONCAT(a, b) ME_PROFILE_CONCAT_INNER(a, b)

#if defined(_MSC_VER)
#define ME_PROFILE_PRETTY_FUNCTION __FUNCSIG__
#else
#define ME_PROFILE_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

namespace minEngine::Profile
{
    /// POD state for scope begin/end (DLL-safe free functions).
    struct ProfileScopeState
    {
        ProfileNameId Name = kInvalidNameId;
        int Active = 0;
    };

    struct ProfilePhaseState
    {
        int Active = 0;
    };

    MINENGINE_API void ProfileScope_Begin(ProfileScopeState& state, const char* staticLiteralName);
    MINENGINE_API void ProfileScope_End(ProfileScopeState& state);
    MINENGINE_API void ProfilePhase_Begin(ProfilePhaseState& state, const char* staticName);
    MINENGINE_API void ProfilePhase_End(ProfilePhaseState& state);

    /// Destructor-only guard. Construction must not call into the DLL beyond storing a pointer
    /// (MinGW has been unreliable with out-of-line ctors that call DLL profile APIs).
    class ProfileScopeEnder
    {
    public:
        explicit ProfileScopeEnder(ProfileScopeState* state) : m_State(state) {}

        ~ProfileScopeEnder()
        {
            if (m_State != nullptr)
            {
                ProfileScope_End(*m_State);
                m_State = nullptr;
            }
        }

        ProfileScopeEnder(const ProfileScopeEnder&) = delete;
        ProfileScopeEnder& operator=(const ProfileScopeEnder&) = delete;

    private:
        ProfileScopeState* m_State = nullptr;
    };

    class ProfilePhaseEnder
    {
    public:
        explicit ProfilePhaseEnder(ProfilePhaseState* state) : m_State(state) {}

        ~ProfilePhaseEnder()
        {
            if (m_State != nullptr)
            {
                ProfilePhase_End(*m_State);
                m_State = nullptr;
            }
        }

        ProfilePhaseEnder(const ProfilePhaseEnder&) = delete;
        ProfilePhaseEnder& operator=(const ProfilePhaseEnder&) = delete;

    private:
        ProfilePhaseState* m_State = nullptr;
    };
}

#if ME_ENABLE_PROFILER
/// Declares a profile scope for the remainder of the current block (UE-style semicolon form).
#define ME_PROFILE_SCOPE(name)                                                                     \
    ::minEngine::Profile::ProfileScopeState ME_PROFILE_CONCAT(_meProfState_, __LINE__);            \
    ::minEngine::Profile::ProfileScope_Begin(ME_PROFILE_CONCAT(_meProfState_, __LINE__), (name));  \
    ::minEngine::Profile::ProfileScopeEnder ME_PROFILE_CONCAT(_meProfEnd_, __LINE__)(               \
        &ME_PROFILE_CONCAT(_meProfState_, __LINE__))

#define ME_PROFILE_FUNCTION() ME_PROFILE_SCOPE(ME_PROFILE_PRETTY_FUNCTION)

#define ME_PROFILE_PHASE(name)                                                                     \
    ::minEngine::Profile::ProfilePhaseState ME_PROFILE_CONCAT(_meProfPhaseState_, __LINE__);       \
    ::minEngine::Profile::ProfilePhase_Begin(                                                      \
        ME_PROFILE_CONCAT(_meProfPhaseState_, __LINE__), (name));                                  \
    ::minEngine::Profile::ProfilePhaseEnder ME_PROFILE_CONCAT(_meProfPhaseEnd_, __LINE__)(          \
        &ME_PROFILE_CONCAT(_meProfPhaseState_, __LINE__))
#else
#define ME_PROFILE_SCOPE(name) do { } while (0)
#define ME_PROFILE_FUNCTION() do { } while (0)
#define ME_PROFILE_PHASE(name) do { } while (0)
#endif
