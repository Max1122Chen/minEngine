#include "MaterialIR.h"
#include "MaterialIRTypes.h"

namespace minEngine
{
    MIRInstruction* AsInstruction(MIRValue* value)
    {
        return (value != nullptr && value->IsInstructionValue()) ? static_cast<MIRInstruction*>(value) : nullptr;
    }

    const MIRInstruction* AsInstruction(const MIRValue* value)
    {
        return (value != nullptr && value->IsInstructionValue()) ? static_cast<const MIRInstruction*>(value) : nullptr;
    }

    bool MIRValue::IsA(MIRValueKind inKind) const
    {
        return Kind == inKind;
    }

    bool MIRValue::IsValid() const
    {
        return !IsPoison();
    }

    bool MIRValue::IsPoison() const
    {
        return IsA(VK_Poison);
    }

    bool MIRValue::IsScalar() const
    {
        return Type != nullptr && Type->AsScalar() != nullptr;
    }

    bool MIRValue::IsVector() const
    {
        return Type != nullptr && Type->AsVector() != nullptr;
    }

    bool MIRValue::IsScalarKind(MIRScalarKind kind) const
    {
        if (!IsScalar())
        {
            return false;
        }
        return Type->AsScalar()->ScalarKind == kind;
    }

    bool MIRValue::IsInstructionValue() const
    {
        return Kind > VK_InstructionBegin && Kind < VK_InstructionEnd;
    }

    bool MIRValue::TypesEqual(const MIRValue* other) const
    {
        if (other == nullptr)
        {
            return false;
        }
        return Type == other->Type;
    }

    std::vector<MIRValue*> MIRValue::GetUses() const
    {
        if (!IsInstructionValue())
        {
            return {};
        }

        const MIRInstruction* instr = static_cast<const MIRInstruction*>(this);
        switch (Kind)
        {
        case VK_SetMaterialOutput:
            return { static_cast<const SetMaterialOutput*>(instr)->Arg };
        case VK_Operator:
        {
            const MIROperator* op = static_cast<const MIROperator*>(instr);
            return { op->Args[0], op->Args[1], op->Args[2] };
        }
        case VK_Cast:
        {
            const MIRCast* cast = static_cast<const MIRCast*>(instr);
            return { cast->Arg };
        }
        case VK_Dimensional:
        {
            const MIRDimensional* dimensional = static_cast<const MIRDimensional*>(instr);
            return dimensional->Components;
        }
        case VK_Subscript:
        {
            const MIRSubscript* subscript = static_cast<const MIRSubscript*>(instr);
            return { subscript->Arg };
        }
        case VK_TextureRead:
        {
            const MIRTextureRead* textureRead = static_cast<const MIRTextureRead*>(instr);
            return { textureRead->Texture, textureRead->TexCoord };
        }
        case VK_Branch:
        {
            const MIRBranch* branch = static_cast<const MIRBranch*>(instr);
            return { branch->Condition, branch->TrueArg, branch->FalseArg };
        }
        default:
            return {};
        }
    }

    std::vector<MIRValue*> MIRValue::GetUsesForStage(ShaderStage stage) const
    {
        (void)stage;
        return GetUses();
    }

    bool MIRConstant::IsBool() const
    {
        return IsScalarKind(SK_Bool);
    }

    bool MIRConstant::IsInt() const
    {
        return IsScalarKind(SK_Int);
    }

    bool MIRConstant::IsFloat() const
    {
        return IsScalarKind(SK_Float);
    }

    MIRBlock* MIRBlock::FindCommonParentWith(MIRBlock* other)
    {
        MIRBlock* a = this;
        MIRBlock* b = other;
        if (a == nullptr || b == nullptr)
        {
            return nullptr;
        }
        if (a == b)
        {
            return a;
        }

        while (a->Level > b->Level)
        {
            a = a->Parent;
        }

        while (b->Level > a->Level)
        {
            b = b->Parent;
        }

        while (a != b)
        {
            a = a->Parent;
            b = b->Parent;
        }

        return a;
    }

    MIRBlock* MIRInstruction::GetDesiredBlockForUse(ShaderStage stage, int32_t useIndex)
    {
        if (Kind == VK_Branch)
        {
            MIRBranch* branch = static_cast<MIRBranch*>(this);
            switch (useIndex)
            {
            case 0: return Block[stage];
            case 1: return &branch->TrueBlock[stage];
            case 2: return &branch->FalseBlock[stage];
            default:    break;
            }
        }

        (void)useIndex;
        return Block[stage];
    }

    MIRDimensional* AsDimensional(MIRValue* value)
    {
        return (value != nullptr && value->IsA(VK_Dimensional)) ? static_cast<MIRDimensional*>(value) : nullptr;
    }

    const MIRDimensional* AsDimensional(const MIRValue* value)
    {
        return (value != nullptr && value->IsA(VK_Dimensional)) ? static_cast<const MIRDimensional*>(value) : nullptr;
    }

    MIRValue* GetScalarComponent(MIRValue* value, int index)
    {
        if (value == nullptr || index < 0)
        {
            return nullptr;
        }

        if (MIRDimensional* dimensional = AsDimensional(value))
        {
            return (index < dimensional->GetNumComponents()) ? dimensional->Components[index] : nullptr;
        }

        if (value->IsScalar() && index == 0)
        {
            return value;
        }

        return nullptr;
    }

    const MIRValue* GetScalarComponent(const MIRValue* value, int index)
    {
        return GetScalarComponent(const_cast<MIRValue*>(value), index);
    }

    MIRPoison* MIRPoison::Get()
    {
        static MIRPoison instance = []() {
            MIRPoison poison{};
            poison.Kind = VK_Poison;
            poison.Type = MIRValueType::GetPoison();
            return poison;
        }();
        return &instance;
    }
}
