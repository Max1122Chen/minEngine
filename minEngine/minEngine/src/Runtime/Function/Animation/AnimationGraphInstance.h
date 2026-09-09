#pragma once

#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Framework/Parameters/ParameterLayout.h"
#include "Runtime/Function/Framework/Parameters/ParameterStore.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace minEngine
{
    // Runtime evaluator for AnimationGraph. Parallel Pose producer to AnimationPlayer.
    class AnimationGraphInstance
    {
    public:
        void SetGraph(const std::shared_ptr<AnimationGraph>& graph);
        AnimationGraph* GetGraph() const { return m_Graph.get(); }

        ParameterStore& GetStore() { return m_Store; }
        const ParameterStore& GetStore() const { return m_Store; }

        bool SetBool(std::string_view name, bool value);
        bool SetInt32(std::string_view name, int32_t value);
        bool SetFloat(std::string_view name, float value);
        bool SetTrigger(std::string_view name);

        void ResetToDefaultState();
        void Update(float deltaSeconds, Pose& outPose);

        const std::string& GetCurrentStateName() const { return m_CurrentStateName; }
        bool IsTransitioning() const { return m_bTransitioning; }
        bool IsBound() const { return m_Graph != nullptr && m_Store.IsBound(); }

    private:
        struct ActiveTransition
        {
            AnimTransition Transition;
            float BlendElapsed = 0.0f;
            float FromTime = 0.0f;
            float ToTime = 0.0f;
        };

        bool EvaluateCondition(const AnimCondition& condition) const;
        bool EvaluateConditions(const std::vector<AnimCondition>& conditions) const;
        const AnimTransition* FindMatchingTransition() const;
        void StartTransition(const AnimTransition& transition);
        void ConsumeTriggersForTransition(const AnimTransition& transition);
        void AdvanceClipTime(const AnimState& state, float& inoutTime, float deltaSeconds) const;
        void EvaluateStatePose(const AnimState& state, float timeSeconds, Pose& outPose) const;

        std::shared_ptr<AnimationGraph> m_Graph;
        std::shared_ptr<const ParameterLayout> m_Layout;
        ParameterStore m_Store;

        std::string m_CurrentStateName;
        float m_CurrentStateTime = 0.0f;
        bool m_bTransitioning = false;
        ActiveTransition m_ActiveTransition;
        Pose m_PoseA;
        Pose m_PoseB;
    };
}
