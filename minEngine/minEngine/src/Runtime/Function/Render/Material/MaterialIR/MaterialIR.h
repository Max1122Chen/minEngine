#pragma once
#include "Core.h"
#include "TypeTraits.h"
#include "../MaterialTypes.h"
#include "MaterialIRTypes.h"
#include <vector>

namespace minEngine
{
    struct MIRInstruction;
    class MIRValueType;

    enum ShaderStage
    {
        Stage_Vertex,
        Stage_Fragment,
        // Stage_Compute,   // Future support for compute shader
        NumStages
    };

    // MIRValue Definition
    
    enum MIRValueKind
    {
        VK_Poison,

        // Values 
        VK_Constant,

        // Instructions
        VK_InstructionBegin,

        VK_Operator,
        VK_Cast,
        VK_Dimensional,
        VK_Subscript,
        VK_ExternalInput,
        VK_TextureObject,
        VK_TextureRead,
        VK_UniformParameter,
        VK_Branch,
        VK_SetMaterialOutput,

        VK_InstructionEnd,
    };

    struct MIRValue
    {
        virtual ~MIRValue() = default;
        MIRValueKind Kind;
        const MIRValueType* Type;

        bool IsA(MIRValueKind inKind) const;
        bool IsValid() const;
        bool IsPoison() const;
        bool IsScalar() const;
        bool IsVector() const;
        bool IsScalarKind(MIRScalarKind kind) const;
        bool IsInstructionValue() const;

        bool TypesEqual(const MIRValue* other) const;

        std::vector<MIRValue*> GetUses() const;
        std::vector<MIRValue*> GetUsesForStage(ShaderStage stage) const;
    };

    MIRInstruction* AsInstruction(MIRValue* value);
    const MIRInstruction* AsInstruction(const MIRValue* value);

    struct MIRDimensional;
    MIRDimensional* AsDimensional(MIRValue* value);
    const MIRDimensional* AsDimensional(const MIRValue* value);
    MIRValue* GetScalarComponent(MIRValue* value, int index);
    const MIRValue* GetScalarComponent(const MIRValue* value, int index);

    template<MIRValueKind TTypeKind>
    struct TMIRValue : public MIRValue
    {
        static constexpr MIRValueKind TypeKind = TTypeKind;
    };

    struct MIRPoison : public TMIRValue<VK_Poison>
    {
        static MIRPoison* Get();
    };

    struct MIRConstant : public TMIRValue<VK_Constant>
    {
        union
        {
            bool BoolValue;
            int64_t IntValue;
            double FloatValue;
        };

        template<typename T>
        T Get() const
        {
            if constexpr (std::is_same_v<T, bool>)
                return BoolValue;
            else if constexpr (std::is_integral_v<T>)
                return IntValue;
            else if constexpr (std::is_floating_point_v<T>)
                return FloatValue;
            else
                static_assert(AlwaysFalse<T>::value, "Unexpected type");
        }

        bool IsBool() const;
        bool IsInt() const;
        bool IsFloat() const;
    };

    struct MIRBlock
    {
        MIRBlock* Parent = nullptr;
        MIRInstruction* FirstInstruction = nullptr;
        int Level = 0;

        MIRBlock* FindCommonParentWith(MIRBlock* other);
    };

    struct MIRInstruction : public MIRValue
    {
        MIRInstruction* Next[NumStages] = {};
        MIRBlock* Block[NumStages] = {};
        uint32_t NumUsers[NumStages] = {};
        uint32_t NumProcessedUsers[NumStages] = {};

        MIRBlock* GetDesiredBlockForUse(ShaderStage stage, int32_t useIndex);
    };

    template<MIRValueKind TTypeKind>
    struct TMIRInstruction : public MIRInstruction
    {
        static constexpr MIRValueKind TypeKind = TTypeKind;
    };

    struct SetMaterialOutput : public TMIRInstruction<VK_SetMaterialOutput>
    {
        // The value to set for the material output (e.g., the computed Albedo color, Metallic value, etc.)
        MIRValue* Arg;
        
        // The material attribute this output corresponds to (e.g., Albedo, Metallic, etc.)
        MaterialProperty Property;
    };

    enum MIROperatorCode
    {
        O_Invalid,

        // Unary
        UO_FirstUO,
        UO_Negative = UO_FirstUO,
        UO_Not,

        // Binary
        BO_FirstBO,
        BO_Add = BO_FirstBO,
        BO_Multiply,
        BO_Subtract,
        BO_Divide,
        BO_Max,
        BO_Min,

        // Ternary
        TO_FirstTO,
        TO_Select = TO_FirstTO,

        OpCount
    };

    struct MIROperator : public TMIRInstruction<VK_Operator>
    {
        MIROperatorCode Op;
        MIRValue* Args[3]; // Support up to ternary operators
    };

    struct MIRCast : public TMIRInstruction<VK_Cast>
    {
        MIRValue* Arg = nullptr;
    };

    // Fixed-length aggregate of scalar SSA values (vectors/matrices). Constants remain scalar-only.
    struct MIRDimensional : public TMIRInstruction<VK_Dimensional>
    {
        std::vector<MIRValue*> Components;

        int GetNumComponents() const { return static_cast<int>(Components.size()); }
    };

    struct MIRSubscript : public TMIRInstruction<VK_Subscript>
    {
        MIRValue* Arg = nullptr;
        int Index = 0;
    };

    enum MIRExternalInputId
    {
        EI_TexCoord0 = 0,
    };

    struct MIRExternalInput : public TMIRInstruction<VK_ExternalInput>
    {
        MIRExternalInputId InputId = EI_TexCoord0;
    };

    struct MIRTextureObject : public TMIRInstruction<VK_TextureObject>
    {
        int TextureSlotIndex = 0;
    };

    struct MIRTextureRead : public TMIRInstruction<VK_TextureRead>
    {
        MIRValue* Texture = nullptr;
        MIRValue* TexCoord = nullptr;
    };

    struct MIRUniformParameter : public TMIRInstruction<VK_UniformParameter>
    {
        int UniformSlotIndex = 0;
        float DefaultValue = 0.0f;
    };

    struct MIRBranch : public TMIRInstruction<VK_Branch>
    {
        MIRValue* Condition = nullptr;
        MIRValue* TrueArg = nullptr;
        MIRValue* FalseArg = nullptr;

        MIRBlock TrueBlock[NumStages];
        MIRBlock FalseBlock[NumStages];
    };
}