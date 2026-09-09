#include "Runtime/Function/Animation/AnimationGraphInstance.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Animation/AnimationClip.h"

#include <cmath>
#include <utility>

namespace minEngine
{
    void AnimationGraphInstance::SetGraph(const std::shared_ptr<AnimationGraph>& graph)
    {
        m_Graph = graph;
        m_Layout.reset();
        m_Store.Clear();
        m_CurrentStateName.clear();
        m_CurrentStateTime = 0.0f;
        m_bTransitioning = false;
        m_ActiveTransition = {};

        if (m_Graph == nullptr)
        {
            return;
        }

        std::string error;
        if (!m_Graph->Validate(&error))
        {
            ME_CORE_ERROR("AnimationGraphInstance::SetGraph rejected: {}", error);
            m_Graph.reset();
            return;
        }

        ParameterLayout layout;
        if (!ParameterLayout::Compile(m_Graph->GetSchema(), layout, &error))
        {
            ME_CORE_ERROR("AnimationGraphInstance::SetGraph layout compile failed: {}", error);
            m_Graph.reset();
            return;
        }

        m_Layout = std::make_shared<const ParameterLayout>(std::move(layout));
        m_Store.BindLayout(m_Layout);
        m_Store.ResetToDefaults();
        ResetToDefaultState();
    }

    bool AnimationGraphInstance::SetBool(std::string_view name, bool value)
    {
        return m_Store.SetBoolByName(name, value);
    }

    bool AnimationGraphInstance::SetInt32(std::string_view name, int32_t value)
    {
        return m_Store.SetInt32ByName(name, value);
    }

    bool AnimationGraphInstance::SetFloat(std::string_view name, float value)
    {
        return m_Store.SetFloatByName(name, value);
    }

    bool AnimationGraphInstance::SetTrigger(std::string_view name)
    {
        return m_Store.SetBoolByName(name, true);
    }

    void AnimationGraphInstance::ResetToDefaultState()
    {
        m_bTransitioning = false;
        m_ActiveTransition = {};
        m_CurrentStateTime = 0.0f;
        m_CurrentStateName.clear();

        if (m_Graph == nullptr)
        {
            return;
        }

        m_CurrentStateName = m_Graph->GetStateMachine().DefaultStateName;
    }

    bool AnimationGraphInstance::EvaluateCondition(const AnimCondition& condition) const
    {
        const ParameterKeyId keyId = m_Store.FindKeyId(condition.ParamName);
        if (keyId == kInvalidParameterKeyId || m_Layout == nullptr)
        {
            return false;
        }

        const ParameterLayoutEntry* entry = m_Layout->GetEntry(keyId);
        if (entry == nullptr)
        {
            return false;
        }

        switch (entry->Type)
        {
        case ParameterValueType::Bool:
        {
            bool value = false;
            if (!m_Store.TryGetBool(keyId, value))
            {
                return false;
            }
            if (condition.Op == AnimConditionOp::IsSet)
            {
                return value;
            }
            if (condition.Op == AnimConditionOp::Equal)
            {
                return value == condition.OperandBool;
            }
            if (condition.Op == AnimConditionOp::NotEqual)
            {
                return value != condition.OperandBool;
            }
            return false;
        }
        case ParameterValueType::Int32:
        {
            int32_t value = 0;
            if (!m_Store.TryGetInt32(keyId, value))
            {
                return false;
            }
            switch (condition.Op)
            {
            case AnimConditionOp::Greater:
                return value > condition.OperandInt;
            case AnimConditionOp::GreaterEqual:
                return value >= condition.OperandInt;
            case AnimConditionOp::Less:
                return value < condition.OperandInt;
            case AnimConditionOp::LessEqual:
                return value <= condition.OperandInt;
            case AnimConditionOp::Equal:
                return value == condition.OperandInt;
            case AnimConditionOp::NotEqual:
                return value != condition.OperandInt;
            case AnimConditionOp::IsSet:
                return value != 0;
            }
            return false;
        }
        case ParameterValueType::Float:
        {
            float value = 0.0f;
            if (!m_Store.TryGetFloat(keyId, value))
            {
                return false;
            }
            switch (condition.Op)
            {
            case AnimConditionOp::Greater:
                return value > condition.OperandFloat;
            case AnimConditionOp::GreaterEqual:
                return value >= condition.OperandFloat;
            case AnimConditionOp::Less:
                return value < condition.OperandFloat;
            case AnimConditionOp::LessEqual:
                return value <= condition.OperandFloat;
            case AnimConditionOp::Equal:
                return value == condition.OperandFloat;
            case AnimConditionOp::NotEqual:
                return value != condition.OperandFloat;
            case AnimConditionOp::IsSet:
                return std::fabs(value) > 1.0e-6f;
            }
            return false;
        }
        }

        return false;
    }

    bool AnimationGraphInstance::EvaluateConditions(const std::vector<AnimCondition>& conditions) const
    {
        for (const AnimCondition& condition : conditions)
        {
            if (!EvaluateCondition(condition))
            {
                return false;
            }
        }
        return true;
    }

    const AnimTransition* AnimationGraphInstance::FindMatchingTransition() const
    {
        if (m_Graph == nullptr)
        {
            return nullptr;
        }

        const AnimStateMachine& sm = m_Graph->GetStateMachine();
        for (const AnimTransition& transition : sm.Transitions)
        {
            if (transition.FromStateName != m_CurrentStateName)
            {
                continue;
            }
            if (EvaluateConditions(transition.Conditions))
            {
                return &transition;
            }
        }

        for (const AnimTransition& transition : sm.AnyStateTransitions)
        {
            if (transition.ToStateName == m_CurrentStateName)
            {
                continue;
            }
            if (EvaluateConditions(transition.Conditions))
            {
                return &transition;
            }
        }

        return nullptr;
    }

    void AnimationGraphInstance::ConsumeTriggersForTransition(const AnimTransition& transition)
    {
        if (m_Layout == nullptr)
        {
            return;
        }

        for (const AnimCondition& condition : transition.Conditions)
        {
            if (condition.Op != AnimConditionOp::IsSet)
            {
                continue;
            }

            const ParameterKeyId keyId = m_Store.FindKeyId(condition.ParamName);
            if (keyId == kInvalidParameterKeyId)
            {
                continue;
            }
            const ParameterLayoutEntry* entry = m_Layout->GetEntry(keyId);
            if (entry != nullptr && entry->Type == ParameterValueType::Bool)
            {
                m_Store.SetBool(keyId, false);
            }
        }
    }

    void AnimationGraphInstance::AdvanceClipTime(const AnimState& state, float& inoutTime, float deltaSeconds) const
    {
        if (state.Clip == nullptr)
        {
            return;
        }

        const float duration = state.Clip->GetDuration();
        if (duration <= 0.0f)
        {
            inoutTime = 0.0f;
            return;
        }

        inoutTime += deltaSeconds * state.Speed;
        if (state.bLoop)
        {
            inoutTime = std::fmod(inoutTime, duration);
            if (inoutTime < 0.0f)
            {
                inoutTime += duration;
            }
        }
        else if (inoutTime > duration)
        {
            inoutTime = duration;
        }
        else if (inoutTime < 0.0f)
        {
            inoutTime = 0.0f;
        }
    }

    void AnimationGraphInstance::EvaluateStatePose(const AnimState& state, float timeSeconds, Pose& outPose) const
    {
        if (state.Clip == nullptr)
        {
            outPose = {};
            return;
        }
        state.Clip->Evaluate(timeSeconds, outPose);
    }

    void AnimationGraphInstance::StartTransition(const AnimTransition& transition)
    {
        m_bTransitioning = true;
        m_ActiveTransition.Transition = transition;
        if (m_ActiveTransition.Transition.FromStateName.empty())
        {
            m_ActiveTransition.Transition.FromStateName = m_CurrentStateName;
        }
        m_ActiveTransition.BlendElapsed = 0.0f;
        m_ActiveTransition.FromTime = m_CurrentStateTime;
        m_ActiveTransition.ToTime = 0.0f;
    }

    void AnimationGraphInstance::Update(float deltaSeconds, Pose& outPose)
    {
        if (!IsBound() || m_Graph == nullptr)
        {
            return;
        }

        if (m_bTransitioning)
        {
            const AnimState* fromState = m_Graph->FindState(
                m_ActiveTransition.Transition.FromStateName.empty()
                    ? m_CurrentStateName
                    : m_ActiveTransition.Transition.FromStateName);
            const AnimState* toState = m_Graph->FindState(m_ActiveTransition.Transition.ToStateName);
            if (fromState == nullptr || toState == nullptr)
            {
                m_bTransitioning = false;
                return;
            }

            AdvanceClipTime(*fromState, m_ActiveTransition.FromTime, deltaSeconds);
            AdvanceClipTime(*toState, m_ActiveTransition.ToTime, deltaSeconds);
            m_ActiveTransition.BlendElapsed += deltaSeconds;

            const float duration = m_ActiveTransition.Transition.BlendDurationSeconds;
            float alpha = 1.0f;
            if (duration > 1.0e-6f)
            {
                alpha = m_ActiveTransition.BlendElapsed / duration;
                if (alpha > 1.0f)
                {
                    alpha = 1.0f;
                }
            }

            EvaluateStatePose(*fromState, m_ActiveTransition.FromTime, m_PoseA);
            EvaluateStatePose(*toState, m_ActiveTransition.ToTime, m_PoseB);
            if (!Pose::Blend(m_PoseA, m_PoseB, alpha, outPose))
            {
                outPose = m_PoseB;
            }

            if (alpha >= 1.0f)
            {
                ConsumeTriggersForTransition(m_ActiveTransition.Transition);
                m_CurrentStateName = m_ActiveTransition.Transition.ToStateName;
                m_CurrentStateTime = m_ActiveTransition.ToTime;
                m_bTransitioning = false;
                m_ActiveTransition = {};
            }
            return;
        }

        const AnimState* currentState = m_Graph->FindState(m_CurrentStateName);
        if (currentState == nullptr)
        {
            return;
        }

        AdvanceClipTime(*currentState, m_CurrentStateTime, deltaSeconds);
        EvaluateStatePose(*currentState, m_CurrentStateTime, outPose);

        if (const AnimTransition* transition = FindMatchingTransition())
        {
            StartTransition(*transition);
            // Zero-duration: finish immediately next lines
            if (transition->BlendDurationSeconds <= 1.0e-6f)
            {
                ConsumeTriggersForTransition(*transition);
                m_CurrentStateName = transition->ToStateName;
                m_CurrentStateTime = 0.0f;
                m_bTransitioning = false;
                m_ActiveTransition = {};
                if (const AnimState* toState = m_Graph->FindState(m_CurrentStateName))
                {
                    EvaluateStatePose(*toState, m_CurrentStateTime, outPose);
                }
            }
        }
    }
}
