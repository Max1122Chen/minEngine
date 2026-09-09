#include "Runtime/Function/Animation/AnimationGraph.h"

#include "Runtime/Function/Animation/Skeleton.h"

namespace minEngine
{
    const AnimState* AnimationGraph::FindState(std::string_view name) const
    {
        for (const AnimState& state : m_StateMachine.States)
        {
            if (state.Name == name)
            {
                return &state;
            }
        }
        return nullptr;
    }

    AnimState* AnimationGraph::FindStateMutable(std::string_view name)
    {
        for (AnimState& state : m_StateMachine.States)
        {
            if (state.Name == name)
            {
                return &state;
            }
        }
        return nullptr;
    }

    bool AnimationGraph::Validate(std::string* outError) const
    {
        auto fail = [&](const char* message) -> bool {
            if (outError != nullptr)
            {
                *outError = message;
            }
            return false;
        };

        std::string schemaError;
        if (!m_Schema.Validate(&schemaError))
        {
            if (outError != nullptr)
            {
                *outError = "Schema: " + schemaError;
            }
            return false;
        }

        if (m_StateMachine.States.empty())
        {
            return fail("AnimationGraph has no states.");
        }

        if (m_StateMachine.DefaultStateName.empty())
        {
            return fail("AnimationGraph DefaultStateName is empty.");
        }

        if (FindState(m_StateMachine.DefaultStateName) == nullptr)
        {
            return fail("DefaultStateName does not match any state.");
        }

        GUID skeletonGuid;
        bool hasSkeletonGuid = false;

        for (const AnimState& state : m_StateMachine.States)
        {
            if (state.Name.empty())
            {
                return fail("AnimState Name is empty.");
            }
            if (state.Clip == nullptr)
            {
                if (outError != nullptr)
                {
                    *outError = "AnimState '" + state.Name + "' has null Clip.";
                }
                return false;
            }

            Skeleton* skeleton = state.Clip->GetSkeleton();
            if (skeleton == nullptr)
            {
                if (outError != nullptr)
                {
                    *outError = "AnimState '" + state.Name + "' clip has no Skeleton.";
                }
                return false;
            }

            if (!hasSkeletonGuid)
            {
                skeletonGuid = skeleton->GetGuid();
                hasSkeletonGuid = true;
            }
            else if (skeleton->GetGuid() != skeletonGuid)
            {
                if (outError != nullptr)
                {
                    *outError = "AnimState '" + state.Name + "' clip Skeleton GUID mismatch.";
                }
                return false;
            }
        }

        // Duplicate state names
        for (size_t i = 0; i < m_StateMachine.States.size(); ++i)
        {
            for (size_t j = i + 1; j < m_StateMachine.States.size(); ++j)
            {
                if (m_StateMachine.States[i].Name == m_StateMachine.States[j].Name)
                {
                    if (outError != nullptr)
                    {
                        *outError = "Duplicate AnimState name '" + m_StateMachine.States[i].Name + "'.";
                    }
                    return false;
                }
            }
        }

        auto validateTransition = [&](const AnimTransition& transition, bool allowEmptyFrom) -> bool {
            if (!allowEmptyFrom && transition.FromStateName.empty())
            {
                return fail("Transition FromStateName is empty.");
            }
            if (!allowEmptyFrom && FindState(transition.FromStateName) == nullptr)
            {
                if (outError != nullptr)
                {
                    *outError = "Transition FromStateName '" + transition.FromStateName + "' not found.";
                }
                return false;
            }
            if (transition.ToStateName.empty() || FindState(transition.ToStateName) == nullptr)
            {
                if (outError != nullptr)
                {
                    *outError = "Transition ToStateName '" + transition.ToStateName + "' not found.";
                }
                return false;
            }
            if (transition.BlendDurationSeconds < 0.0f)
            {
                return fail("Transition BlendDurationSeconds is negative.");
            }

            for (const AnimCondition& condition : transition.Conditions)
            {
                if (condition.ParamName.empty())
                {
                    return fail("Transition condition ParamName is empty.");
                }
                if (m_Schema.FindEntryIndex(condition.ParamName) == SIZE_MAX)
                {
                    if (outError != nullptr)
                    {
                        *outError = "Condition param '" + condition.ParamName + "' not in Schema.";
                    }
                    return false;
                }
            }
            return true;
        };

        for (const AnimTransition& transition : m_StateMachine.Transitions)
        {
            if (!validateTransition(transition, false))
            {
                return false;
            }
        }

        for (const AnimTransition& transition : m_StateMachine.AnyStateTransitions)
        {
            if (!validateTransition(transition, true))
            {
                return false;
            }
        }

        return true;
    }
}
