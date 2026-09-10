#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    ME_ENUM()
    enum class AnimConditionOp : uint8_t
    {
        Greater = 0,
        GreaterEqual = 1,
        Less = 2,
        LessEqual = 3,
        Equal = 4,
        NotEqual = 5,
        IsSet = 6, // Trigger / Bool raised
    };

    ME_STRUCT()
    struct AnimCondition
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string ParamName;

        ME_PROPERTY()
        AnimConditionOp Op = AnimConditionOp::Greater;

        ME_PROPERTY()
        float OperandFloat = 0.0f;

        ME_PROPERTY()
        int32_t OperandInt = 0;

        ME_PROPERTY()
        bool OperandBool = false;
    };

    ME_STRUCT()
    struct AnimState
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string Name;

        ME_PROPERTY()
        std::shared_ptr<AnimationClip> Clip;

        ME_PROPERTY()
        bool bLoop = true;

        ME_PROPERTY()
        float Speed = 1.0f;

        ME_PROPERTY()
        float EditorPosX = 0.0f;

        ME_PROPERTY()
        float EditorPosY = 0.0f;
    };

    ME_STRUCT()
    struct AnimTransition
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string FromStateName;

        ME_PROPERTY()
        std::string ToStateName;

        ME_PROPERTY()
        std::vector<AnimCondition> Conditions;

        ME_PROPERTY()
        float BlendDurationSeconds = 0.15f;
    };

    ME_STRUCT()
    struct AnimStateMachine
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::vector<AnimState> States;

        ME_PROPERTY()
        std::vector<AnimTransition> Transitions;

        // Evaluated after explicit From->To edges when not transitioning.
        ME_PROPERTY()
        std::vector<AnimTransition> AnyStateTransitions;

        ME_PROPERTY()
        std::string DefaultStateName;
    };

    ME_CLASS()
    class AnimationGraph : public Asset
    {
        ME_GENERATED_BODY()
    public:
        AnimationGraph() = default;
        ~AnimationGraph() override = default;

        ParameterSchema& GetSchema() { return m_Schema; }
        const ParameterSchema& GetSchema() const { return m_Schema; }
        void SetSchema(ParameterSchema schema) { m_Schema = std::move(schema); }

        AnimStateMachine& GetStateMachine() { return m_StateMachine; }
        const AnimStateMachine& GetStateMachine() const { return m_StateMachine; }
        void SetStateMachine(AnimStateMachine stateMachine) { m_StateMachine = std::move(stateMachine); }

        // Returns false and fills outError on first validation failure.
        bool Validate(std::string* outError = nullptr) const;

        const AnimState* FindState(std::string_view name) const;
        AnimState* FindStateMutable(std::string_view name);

    private:
        ME_PROPERTY()
        ParameterSchema m_Schema;

        ME_PROPERTY()
        AnimStateMachine m_StateMachine;
    };
}

#include "Generated/Reflection/AnimationGraph.gen.h"
