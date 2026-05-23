#include "MIREmitter.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialPropertyUtil.h"
#include "MIRBuilder.h"
#include "MIRGraph.h"
#include "MaterialIRTypes.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    namespace
    {
        MIRConstant* AsConstant(MIRValue* value)
        {
            return value != nullptr ? dynamic_cast<MIRConstant*>(value) : nullptr;
        }

        MIRValue* TryFoldUnary(MIREmitter& emitter, MIROperatorCode op, MIRConstant* constant)
        {
            if (constant == nullptr)
            {
                return nullptr;
            }

            switch (op)
            {
            case UO_Negative:
                if (constant->IsFloat())
                {
                    return emitter.ConstantFloat(-static_cast<float>(constant->Get<double>()));
                }
                if (constant->IsInt())
                {
                    return emitter.ConstantInt(-constant->Get<int64_t>());
                }
                break;
            case UO_Not:
                if (constant->IsBool())
                {
                    return emitter.ConstantBool(!constant->Get<bool>());
                }
                if (constant->IsInt())
                {
                    return emitter.ConstantBool(constant->Get<int64_t>() == 0);
                }
                if (constant->IsFloat())
                {
                    return emitter.ConstantBool(constant->Get<double>() == 0.0);
                }
                break;
            default:
                break;
            }

            return nullptr;
        }

        MIRValue* TryFoldBinary(MIREmitter& emitter, MIROperatorCode op, MIRConstant* left, MIRConstant* right,
                                const MIRPrimitiveType* resultType)
        {
            if (left == nullptr || right == nullptr || resultType == nullptr || !resultType->IsScalar())
            {
                return nullptr;
            }

            if (resultType->ScalarKind == SK_Float)
            {
                const float a = static_cast<float>(left->Get<double>());
                const float b = static_cast<float>(right->Get<double>());
                switch (op)
                {
                case BO_Add:
                    return emitter.ConstantFloat(a + b);
                case BO_Multiply:
                    return emitter.ConstantFloat(a * b);
                case BO_Subtract:
                    return emitter.ConstantFloat(a - b);
                case BO_Divide:
                    if (b == 0.0f)
                    {
                        emitter.EmitDiagnostic("Divide by zero.");
                        return emitter.Poison();
                    }
                    return emitter.ConstantFloat(a / b);
                case BO_Max:
                    return emitter.ConstantFloat(std::max(a, b));
                case BO_Min:
                    return emitter.ConstantFloat(std::min(a, b));
                default:
                    break;
                }
            }

            if (resultType->ScalarKind == SK_Int)
            {
                const int64_t a = left->Get<int64_t>();
                const int64_t b = right->Get<int64_t>();
                switch (op)
                {
                case BO_Add:
                    return emitter.ConstantInt(a + b);
                case BO_Multiply:
                    return emitter.ConstantInt(a * b);
                case BO_Subtract:
                    return emitter.ConstantInt(a - b);
                case BO_Divide:
                    if (b == 0)
                    {
                        emitter.EmitDiagnostic("Divide by zero.");
                        return emitter.Poison();
                    }
                    return emitter.ConstantInt(a / b);
                case BO_Max:
                    return emitter.ConstantInt(std::max(a, b));
                case BO_Min:
                    return emitter.ConstantInt(std::min(a, b));
                default:
                    break;
                }
            }

            return nullptr;
        }

        MIRValue* TryFoldTernary(MIREmitter& emitter, MIRConstant* condition, MIRValue* trueValue, MIRValue* falseValue)
        {
            if (condition == nullptr || !condition->IsBool())
            {
                return nullptr;
            }

            return condition->Get<bool>() ? trueValue : falseValue;
        }

        bool TryUnpackConstantDimensional(MIRValue* value, std::vector<MIRConstant*>& outComponents)
        {
            MIRDimensional* dimensional = AsDimensional(value);
            if (dimensional == nullptr)
            {
                return false;
            }

            outComponents.clear();
            outComponents.reserve(dimensional->Components.size());
            for (MIRValue* component : dimensional->Components)
            {
                MIRConstant* constant = AsConstant(component);
                if (constant == nullptr)
                {
                    return false;
                }
                outComponents.push_back(constant);
            }

            return true;
        }

        bool IsUnaryOperator(MIROperatorCode op)
        {
            return op > O_Invalid && op < BO_FirstBO;
        }

        bool IsBinaryOperator(MIROperatorCode op)
        {
            return op >= BO_FirstBO && op < TO_FirstTO;
        }

        bool IsTernaryOperator(MIROperatorCode op)
        {
            return op >= TO_FirstTO && op < OpCount;
        }

        bool IsComponentwiseOperator(MIROperatorCode op)
        {
            return IsUnaryOperator(op) || IsBinaryOperator(op) || op == TO_Select;
        }

        enum MIROperatorParameterFilter : uint32_t
        {
            OPTF_None = 0,
            OPTF_CastToCommonType = 1 << 0,
            OPTF_CastToBool = 1 << 1,
        };

        enum MIROperatorReturnType
        {
            ORT_FirstArgumentType,
            ORT_Boolean,
        };

        struct MIROperatorSignature
        {
            MIROperatorParameterFilter ParameterFilters[3] = {};
            MIROperatorReturnType ReturnType = ORT_FirstArgumentType;
        };

        const MIROperatorSignature& GetOperatorSignature(MIROperatorCode op)
        {
            static MIROperatorSignature signatures[OpCount] = {};
            static bool initialized = false;
            if (!initialized)
            {
                const MIROperatorSignature unaryArithmetic = {
                    { OPTF_None, OPTF_None, OPTF_None },
                    ORT_FirstArgumentType,
                };
                const MIROperatorSignature unaryBoolean = {
                    { OPTF_CastToBool, OPTF_None, OPTF_None },
                    ORT_Boolean,
                };
                const MIROperatorSignature binaryArithmetic = {
                    { OPTF_CastToCommonType, OPTF_CastToCommonType, OPTF_None },
                    ORT_FirstArgumentType,
                };
                const MIROperatorSignature ternarySelect = {
                    { OPTF_CastToBool, OPTF_CastToCommonType, OPTF_CastToCommonType },
                    ORT_FirstArgumentType,
                };

                signatures[UO_Negative] = unaryArithmetic;
                signatures[UO_Not] = unaryBoolean;
                signatures[BO_Add] = binaryArithmetic;
                signatures[BO_Multiply] = binaryArithmetic;
                signatures[BO_Subtract] = binaryArithmetic;
                signatures[BO_Divide] = binaryArithmetic;
                signatures[BO_Max] = binaryArithmetic;
                signatures[BO_Min] = binaryArithmetic;
                signatures[TO_Select] = ternarySelect;
                initialized = true;
            }

            return signatures[op];
        }

        MIRValue* TryFoldOperatorScalar(
            MIREmitter& emitter,
            MIROperatorCode op,
            MIRValue* a,
            MIRValue* b,
            MIRValue* c,
            const MIRPrimitiveType* scalarType)
        {
            if (scalarType == nullptr || !scalarType->IsScalar())
            {
                return nullptr;
            }

            if (IsUnaryOperator(op))
            {
                return TryFoldUnary(emitter, op, AsConstant(a));
            }

            if (IsBinaryOperator(op))
            {
                return TryFoldBinary(emitter, op, AsConstant(a), AsConstant(b), scalarType);
            }

            if (IsTernaryOperator(op))
            {
                return TryFoldTernary(emitter, AsConstant(a), b, c);
            }

            return nullptr;
        }

    }

    static void InitializeMIRInstruction(MIRInstruction* instruction, MIRValueKind valueKind, const MIRValueType* resultType)
    {
        instruction->Kind = valueKind;
        instruction->Type = resultType;
        for (int stage = 0; stage < NumStages; ++stage)
        {
            instruction->Next[stage] = nullptr;
            instruction->Block[stage] = nullptr;
            instruction->NumUsers[stage] = 0;
            instruction->NumProcessedUsers[stage] = 0;
        }
    }

    MIRValue* MIREmitter::CastConstant(MIREmitter& emitter, MIRConstant* constant, MIRScalarKind sourceKind,
                                       MIRScalarKind targetKind)
    {
        if (sourceKind == targetKind)
        {
            return constant;
        }

        switch (sourceKind)
        {
        case SK_Bool:
        case SK_Int:
        {
            const int64_t intValue = constant->IsBool() ? static_cast<int64_t>(constant->Get<bool>())
                                                        : constant->Get<int64_t>();
            switch (targetKind)
            {
            case SK_Int:
                return emitter.ConstantInt(intValue);
            case SK_Float:
                return emitter.ConstantFloat(static_cast<float>(intValue));
            default:
                break;
            }
            break;
        }
        case SK_Float:
        {
            const float floatValue = static_cast<float>(constant->Get<double>());
            switch (targetKind)
            {
            case SK_Bool:
                return emitter.ConstantBool(floatValue != 0.0f);
            case SK_Int:
                return emitter.ConstantInt(static_cast<int64_t>(floatValue));
            default:
                break;
            }
            break;
        }
        default:
            break;
        }

        return emitter.Poison();
    }

    MIRValue* MIREmitter::CastValueToPrimitiveType(MIRValue* value, const MIRPrimitiveType* targetPrimitiveType)
    {
        if (value == nullptr || value->IsPoison())
        {
            return value;
        }

        const MIRPrimitiveType* valuePrimitiveType = value->Type->AsPrimitive();
        if (valuePrimitiveType == nullptr || targetPrimitiveType == nullptr)
        {
            EmitDiagnostic("Cast requires primitive types.");
            return Poison();
        }

        if (valuePrimitiveType == targetPrimitiveType)
        {
            return value;
        }

        if (targetPrimitiveType->IsMatrix() || valuePrimitiveType->IsMatrix())
        {
            EmitDiagnostic("Matrix casts are not implemented.");
            return Poison();
        }

        const MIRPrimitiveType* targetScalarType = targetPrimitiveType->IsScalar()
            ? targetPrimitiveType
            : MIRPrimitiveType::Get(targetPrimitiveType->ScalarKind, 1, 1);

        if (targetPrimitiveType->IsVector())
        {
            if (valuePrimitiveType->IsScalar())
            {
                MIRValue* component = CastValueToPrimitiveType(value, targetScalarType);
                if (component == nullptr || component->IsPoison())
                {
                    return Poison();
                }

                std::vector<MIRValue*> components(static_cast<size_t>(targetPrimitiveType->NumRows), component);
                return MakeDimensional(targetPrimitiveType, std::move(components));
            }

            if (valuePrimitiveType->IsVector())
            {
                std::vector<MIRValue*> components;
                components.reserve(static_cast<size_t>(targetPrimitiveType->NumRows));
                const int sharedComponents = std::min(valuePrimitiveType->NumRows, targetPrimitiveType->NumRows);
                for (int componentIndex = 0; componentIndex < sharedComponents; ++componentIndex)
                {
                    MIRValue* sourceComponent = GetScalarComponent(value, componentIndex);
                    if (sourceComponent == nullptr)
                    {
                        sourceComponent = Subscript(value, componentIndex);
                    }
                    components.push_back(CastValueToPrimitiveType(sourceComponent, targetScalarType));
                }

                for (int componentIndex = sharedComponents; componentIndex < targetPrimitiveType->NumRows; ++componentIndex)
                {
                    components.push_back(ConstantZero(targetScalarType));
                }

                return MakeDimensional(targetPrimitiveType, std::move(components));
            }
        }

        if (targetPrimitiveType->IsScalar())
        {
            if (valuePrimitiveType->IsVector())
            {
                MIRValue* firstComponent = Subscript(value, 0);
                if (firstComponent == nullptr || firstComponent->IsPoison())
                {
                    return Poison();
                }
                return CastValueToPrimitiveType(firstComponent, targetPrimitiveType);
            }

            if (MIRConstant* constant = AsConstant(value))
            {
                return CastConstant(*this, constant, valuePrimitiveType->ScalarKind, targetPrimitiveType->ScalarKind);
            }

            MIRCast* instruction = m_Graph->CreateValue<MIRCast>();
            InitializeMIRInstruction(instruction, VK_Cast, targetPrimitiveType);
            instruction->Arg = value;
            return instruction;
        }

        EmitDiagnostic("Unsupported cast target.");
        return Poison();
    }

    MIRValue* MIREmitter::MakeDimensional(const MIRPrimitiveType* type, std::vector<MIRValue*> components)
    {
        if (type == nullptr || !type->IsVector())
        {
            EmitDiagnostic("MakeDimensional requires a vector type.");
            return Poison();
        }

        if (static_cast<int>(components.size()) != type->NumRows)
        {
            EmitDiagnostic("MakeDimensional component count mismatch.");
            return Poison();
        }

        for (MIRValue* component : components)
        {
            if (component == nullptr || component->IsPoison())
            {
                return Poison();
            }
        }

        MIRDimensional* dimensional = m_Graph->CreateValue<MIRDimensional>();
        InitializeMIRInstruction(dimensional, VK_Dimensional, type);
        dimensional->Components = std::move(components);
        return dimensional;
    }

    MIRValue* MIREmitter::Vector3(MIRValue* x, MIRValue* y, MIRValue* z)
    {
        if (x == nullptr || y == nullptr || z == nullptr)
        {
            EmitDiagnostic("Vector3 input is null.");
            return Poison();
        }

        const MIRValueType* commonType = GetCommonType(GetCommonType(x->Type, y->Type), z->Type);
        const MIRPrimitiveType* commonScalar = commonType != nullptr ? commonType->AsScalar() : nullptr;
        if (commonScalar == nullptr)
        {
            return Poison();
        }

        x = Cast(x, commonScalar);
        y = Cast(y, commonScalar);
        z = Cast(z, commonScalar);
        if (x == nullptr || x->IsPoison() || y == nullptr || y->IsPoison() || z == nullptr || z->IsPoison())
        {
            return Poison();
        }

        const MIRPrimitiveType* vectorType = MIRPrimitiveType::Get(commonScalar->ScalarKind, 3, 1);
        return MakeDimensional(vectorType, { x, y, z });
    }

    MIRValue* MIREmitter::ConstantFloat3(float r, float g, float b)
    {
        return Vector3(ConstantFloat(r), ConstantFloat(g), ConstantFloat(b));
    }

    MIRValue* MIREmitter::ConstantZero(const MIRPrimitiveType* scalarType)
    {
        if (scalarType == nullptr || !scalarType->IsScalar())
        {
            return Poison();
        }

        switch (scalarType->ScalarKind)
        {
        case SK_Bool:
            return ConstantBool(false);
        case SK_Int:
            return ConstantInt(0);
        case SK_Float:
            return ConstantFloat(0.0f);
        default:
            return Poison();
        }
    }

    MIRValue* MIREmitter::ConstantDefaultForProperty(MaterialProperty property)
    {
        switch (property)
        {
        case MP_Albedo:
        case MP_Emissive:
            return ConstantFloat3(0.0f, 0.0f, 0.0f);
        default:
            return ConstantFloat(GetMaterialPropertyDefault(property));
        }
    }

    MIRValue* MIREmitter::Poison()
    {
        return MIRPoison::Get();
    }

    void MIREmitter::EmitDiagnostic(const std::string& message)
    {
        if (m_Graph == nullptr)
        {
            return;
        }

        if (m_CurrentNodeDef != nullptr)
        {
            const Reflection::MEClass* nodeClass = m_CurrentNodeDef->GetClass();
            if (nodeClass != nullptr && !nodeClass->GetName().empty())
            {
                m_Graph->AddDiagnostic(nodeClass->GetName() + ": " + message);
                return;
            }
        }

        m_Graph->AddDiagnostic(message);
    }

    MIRValue* MIREmitter::ConstantBool(bool value)
    {
        MIRConstant* constant = m_Graph->CreateValue<MIRConstant>();
        constant->Kind = VK_Constant;
        constant->Type = MIRPrimitiveType::GetBool();
        constant->BoolValue = value;
        return constant;
    }

    MIRValue* MIREmitter::ConstantFloat(float value)
    {
        MIRConstant* constant = m_Graph->CreateValue<MIRConstant>();
        constant->Kind = VK_Constant;
        constant->Type = MIRPrimitiveType::GetFloat();
        constant->FloatValue = value;
        return constant;
    }

    MIRValue* MIREmitter::ConstantInt(int64_t value)
    {
        MIRConstant* constant = m_Graph->CreateValue<MIRConstant>();
        constant->Kind = VK_Constant;
        constant->Type = MIRPrimitiveType::GetInt();
        constant->IntValue = value;
        return constant;
    }

    const MIRValueType* MIREmitter::TryGetCommonType(const MIRValueType* a, const MIRValueType* b) const
    {
        if (a == b)
        {
            return a;
        }

        const MIRPrimitiveType* primitiveA = a != nullptr ? a->AsPrimitive() : nullptr;
        const MIRPrimitiveType* primitiveB = b != nullptr ? b->AsPrimitive() : nullptr;
        if (primitiveA == nullptr || primitiveB == nullptr)
        {
            return nullptr;
        }

        if (primitiveA->IsMatrix() || primitiveB->IsMatrix())
        {
            return nullptr;
        }

        const MIRScalarKind scalarKind = static_cast<MIRScalarKind>(std::max(
            static_cast<int>(primitiveA->ScalarKind),
            static_cast<int>(primitiveB->ScalarKind)));
        const int numRows = std::max(primitiveA->NumRows, primitiveB->NumRows);
        return MIRPrimitiveType::Get(scalarKind, numRows, 1);
    }

    const MIRValueType* MIREmitter::GetCommonType(const MIRValueType* a, const MIRValueType* b)
    {
        if (const MIRValueType* commonType = TryGetCommonType(a, b))
        {
            return commonType;
        }

        EmitDiagnostic("No common type between operands.");
        return nullptr;
    }

    MIRValue* MIREmitter::Cast(MIRValue* value, const MIRValueType* targetType)
    {
        if (value == nullptr || value->IsPoison())
        {
            return value;
        }

        if (targetType == nullptr || targetType->IsPoison())
        {
            EmitDiagnostic("Cast target type is invalid.");
            return Poison();
        }

        if (value->Type == targetType)
        {
            return value;
        }

        if (const MIRPrimitiveType* targetPrimitive = targetType->AsPrimitive())
        {
            return CastValueToPrimitiveType(value, targetPrimitive);
        }

        EmitDiagnostic("Cast target must be a primitive type.");
        return Poison();
    }

    MIRValue* MIREmitter::Input(const MaterialGraphNodeDefInput* input)
    {
        if (input == nullptr || m_Builder == nullptr)
        {
            return Poison();
        }
        return m_Builder->FetchValueFromInput(input);
    }

    void MIREmitter::Output(int outputIndex, MIRValue* value)
    {
        if (m_CurrentNodeDef == nullptr)
        {
            return;
        }
        Output(m_CurrentNodeDef->GetOutput(outputIndex), value);
    }

    void MIREmitter::Output(const MaterialGraphNodeDefOutput* output, MIRValue* value)
    {
        if (output == nullptr || m_Builder == nullptr)
        {
            return;
        }
        m_Builder->BindValueToOutput(output, value);
    }

    bool MIREmitter::IsOutputConnected(int outputIndex) const
    {
        return m_Builder != nullptr && m_Builder->IsOutputConnected(outputIndex);
    }

    MIRValue* MIREmitter::TryFoldOperator(
        MIROperatorCode op,
        MIRValue* a,
        MIRValue* b,
        MIRValue* c,
        const MIRPrimitiveType* resultType)
    {
        if (resultType == nullptr)
        {
            return nullptr;
        }

        if (resultType->IsScalar())
        {
            return TryFoldOperatorScalar(*this, op, a, b, c, resultType);
        }

        if (!IsComponentwiseOperator(op))
        {
            return nullptr;
        }

        const int numComponents = resultType->GetNumComponents();
        const MIRPrimitiveType* scalarType = MIRPrimitiveType::Get(resultType->ScalarKind, 1, 1);

        bool someComponentFolded = false;
        std::vector<MIRValue*> foldedComponents(static_cast<size_t>(numComponents), nullptr);

        for (int componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            MIRValue* aComponent = Subscript(a, componentIndex);
            MIRValue* bComponent = b != nullptr ? Subscript(b, componentIndex) : nullptr;
            MIRValue* cComponent = c != nullptr ? Subscript(c, componentIndex) : nullptr;

            if (MIRValue* folded = TryFoldOperatorScalar(*this, op, aComponent, bComponent, cComponent, scalarType))
            {
                someComponentFolded = true;
                foldedComponents[componentIndex] = folded;
            }
        }

        if (!someComponentFolded)
        {
            return nullptr;
        }

        if (resultType->IsScalar())
        {
            return foldedComponents[0];
        }

        std::vector<MIRValue*> resultComponents;
        resultComponents.reserve(static_cast<size_t>(numComponents));
        for (int componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            if (foldedComponents[componentIndex] != nullptr)
            {
                resultComponents.push_back(foldedComponents[componentIndex]);
                continue;
            }

            resultComponents.push_back(EmitOperator(
                scalarType,
                op,
                Subscript(a, componentIndex),
                b != nullptr ? Subscript(b, componentIndex) : nullptr,
                c != nullptr ? Subscript(c, componentIndex) : nullptr));
        }

        return MakeDimensional(resultType, std::move(resultComponents));
    }

    const MIRPrimitiveType* MIREmitter::ValidateOperatorAndGetResultType(
        MIROperatorCode op,
        MIRValue*& a,
        MIRValue*& b,
        MIRValue*& c)
    {
        if (a == nullptr)
        {
            EmitDiagnostic("Operator primary input is null.");
            return nullptr;
        }

        if (IsBinaryOperator(op) && b == nullptr)
        {
            EmitDiagnostic("Binary operator secondary input is null.");
            return nullptr;
        }

        if (IsTernaryOperator(op) && (b == nullptr || c == nullptr))
        {
            EmitDiagnostic("Ternary operator missing operands.");
            return nullptr;
        }

        if (IsUnaryOperator(op) && (b != nullptr || c != nullptr))
        {
            EmitDiagnostic("Unary operator received extra operands.");
            return nullptr;
        }

        const MIRPrimitiveType* firstPrimitiveType = a->Type->AsPrimitive();
        if (firstPrimitiveType == nullptr)
        {
            EmitDiagnostic("Operator argument must be a primitive type.");
            return nullptr;
        }

        MIRValue* arguments[3] = { a, b, c };
        const MIROperatorSignature& signature = GetOperatorSignature(op);

        if (op == TO_Select)
        {
            arguments[0] = Cast(arguments[0], MIRPrimitiveType::GetBool());
            if (arguments[0] == nullptr || arguments[0]->IsPoison())
            {
                return nullptr;
            }

            const MIRValueType* branchCommonType = GetCommonType(arguments[1]->Type, arguments[2]->Type);
            const MIRPrimitiveType* branchPrimitive = branchCommonType != nullptr ? branchCommonType->AsPrimitive() : nullptr;
            if (branchPrimitive == nullptr)
            {
                return nullptr;
            }

            arguments[1] = Cast(arguments[1], branchPrimitive);
            arguments[2] = Cast(arguments[2], branchPrimitive);
            if (arguments[1] == nullptr || arguments[1]->IsPoison() || arguments[2] == nullptr || arguments[2]->IsPoison())
            {
                return nullptr;
            }

            a = arguments[0];
            b = arguments[1];
            c = arguments[2];
            return branchPrimitive;
        }

        const MIRValueType* commonType = firstPrimitiveType;
        for (int argumentIndex = 1; argumentIndex < 3 && arguments[argumentIndex] != nullptr; ++argumentIndex)
        {
            const MIRPrimitiveType* argumentPrimitive = arguments[argumentIndex]->Type->AsPrimitive();
            if (argumentPrimitive == nullptr)
            {
                EmitDiagnostic("Operator argument must be a primitive type.");
                return nullptr;
            }

            commonType = TryGetCommonType(commonType, argumentPrimitive);
            if (commonType == nullptr)
            {
                EmitDiagnostic("No common type between operands.");
                return nullptr;
            }
        }

        for (int argumentIndex = 0; argumentIndex < 3 && arguments[argumentIndex] != nullptr; ++argumentIndex)
        {
            const MIROperatorParameterFilter filter = signature.ParameterFilters[argumentIndex];
            if (filter & OPTF_CastToBool)
            {
                arguments[argumentIndex] = Cast(arguments[argumentIndex], MIRPrimitiveType::GetBool());
            }
            else if (filter & OPTF_CastToCommonType)
            {
                arguments[argumentIndex] = Cast(arguments[argumentIndex], commonType);
            }

            if (arguments[argumentIndex] == nullptr || arguments[argumentIndex]->IsPoison())
            {
                return nullptr;
            }
        }

        a = arguments[0];
        b = arguments[1];
        c = arguments[2];

        switch (signature.ReturnType)
        {
        case ORT_Boolean:
            return MIRPrimitiveType::GetBool();
        case ORT_FirstArgumentType:
        default:
            return a->Type->AsPrimitive();
        }
    }

    MIRValue* MIREmitter::EmitSubscriptInstruction(MIRValue* value, int index)
    {
        if (value == nullptr || value->IsPoison())
        {
            return value;
        }

        const MIRPrimitiveType* primitiveType = value->Type->AsPrimitive();
        if (primitiveType == nullptr || !primitiveType->IsVector())
        {
            EmitDiagnostic("Subscript requires a vector value.");
            return Poison();
        }

        if (index < 0 || index >= primitiveType->GetNumComponents())
        {
            EmitDiagnostic("Subscript index is out of range.");
            return Poison();
        }

        const MIRPrimitiveType* scalarType = MIRPrimitiveType::Get(primitiveType->ScalarKind, 1, 1);
        MIRSubscript* instruction = m_Graph->CreateValue<MIRSubscript>();
        InitializeMIRInstruction(instruction, VK_Subscript, scalarType);
        instruction->Arg = value;
        instruction->Index = index;
        return instruction;
    }

    MIRValue* MIREmitter::SubscriptChannel(MIRValue* value, int channelIndex)
    {
        return EmitSubscriptInstruction(value, channelIndex);
    }

    MIRValue* MIREmitter::Subscript(MIRValue* value, int index)
    {
        if (value == nullptr || value->IsPoison())
        {
            return value;
        }

        const MIRPrimitiveType* primitiveType = value->Type->AsPrimitive();
        if (primitiveType == nullptr)
        {
            EmitDiagnostic("Subscript requires a primitive value.");
            return Poison();
        }

        if (index == 0 && primitiveType->IsScalar())
        {
            return value;
        }

        if (index < 0 || index >= primitiveType->GetNumComponents())
        {
            EmitDiagnostic("Subscript index is out of range.");
            return Poison();
        }

        if (MIRValue* component = GetScalarComponent(value, index))
        {
            return component;
        }

        return EmitSubscriptInstruction(value, index);
    }

    MIRValue* MIREmitter::EmitOperator(
        const MIRPrimitiveType* resultType,
        MIROperatorCode op,
        MIRValue* a,
        MIRValue* b,
        MIRValue* c)
    {
        if (resultType == nullptr || a == nullptr)
        {
            return Poison();
        }

        MIROperator* instruction = m_Graph->CreateValue<MIROperator>();
        InitializeMIRInstruction(instruction, VK_Operator, resultType);
        instruction->Op = op;
        instruction->Args[0] = a;
        instruction->Args[1] = b;
        instruction->Args[2] = c;
        return instruction;
    }

    MIRValue* MIREmitter::Operator(MIROperatorCode op, MIRValue* a, MIRValue* b, MIRValue* c)
    {
        const MIRPrimitiveType* resultType = ValidateOperatorAndGetResultType(op, a, b, c);
        if (resultType == nullptr)
        {
            return Poison();
        }

        if (MIRValue* folded = TryFoldOperator(op, a, b, c, resultType))
        {
            return folded;
        }

        return EmitOperator(resultType, op, a, b, c);
    }

    MIRValue* MIREmitter::Negative(MIRValue* value)
    {
        return Operator(UO_Negative, value, nullptr, nullptr);
    }

    MIRValue* MIREmitter::Not(MIRValue* value)
    {
        return Operator(UO_Not, value, nullptr, nullptr);
    }

    MIRValue* MIREmitter::Add(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Add, left, right, nullptr);
    }

    MIRValue* MIREmitter::Subtract(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Subtract, left, right, nullptr);
    }

    MIRValue* MIREmitter::Multiply(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Multiply, left, right, nullptr);
    }

    MIRValue* MIREmitter::Divide(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Divide, left, right, nullptr);
    }

    MIRValue* MIREmitter::Max(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Max, left, right, nullptr);
    }

    MIRValue* MIREmitter::Min(MIRValue* left, MIRValue* right)
    {
        return Operator(BO_Min, left, right, nullptr);
    }

    MIRValue* MIREmitter::Select(MIRValue* condition, MIRValue* trueValue, MIRValue* falseValue)
    {
        return Operator(TO_Select, condition, trueValue, falseValue);
    }

    MIRValue* MIREmitter::Branch(MIRValue* condition, MIRValue* trueValue, MIRValue* falseValue)
    {
        if (condition == nullptr || trueValue == nullptr || falseValue == nullptr)
        {
            EmitDiagnostic("Branch input is null.");
            return Poison();
        }

        condition = Cast(condition, MIRPrimitiveType::GetBool());
        if (condition == nullptr || condition->IsPoison())
        {
            EmitDiagnostic("Branch condition is invalid.");
            return Poison();
        }

        if (MIRConstant* constCondition = dynamic_cast<MIRConstant*>(condition))
        {
            return constCondition->Get<bool>() ? trueValue : falseValue;
        }

        const MIRValueType* commonType = GetCommonType(trueValue->Type, falseValue->Type);
        if (commonType == nullptr)
        {
            EmitDiagnostic("Branch true/false values have no common type.");
            return Poison();
        }

        trueValue = Cast(trueValue, commonType);
        falseValue = Cast(falseValue, commonType);
        if (trueValue == nullptr || trueValue->IsPoison() || falseValue == nullptr || falseValue->IsPoison())
        {
            EmitDiagnostic("Branch true/false value is invalid.");
            return Poison();
        }

        MIRBranch* instruction = m_Graph->CreateValue<MIRBranch>();
        InitializeMIRInstruction(instruction, VK_Branch, commonType);
        instruction->Condition = condition;
        instruction->TrueArg = trueValue;
        instruction->FalseArg = falseValue;
        return instruction;
    }

    MIRValue* MIREmitter::ExternalInput(MIRExternalInputId inputId)
    {
        const MIRPrimitiveType* resultType = MIRPrimitiveType::GetFloat2();
        if (inputId == EI_WorldNormal)
        {
            resultType = MIRPrimitiveType::GetFloat3();
        }

        MIRExternalInput* instruction = m_Graph->CreateValue<MIRExternalInput>();
        InitializeMIRInstruction(instruction, VK_ExternalInput, resultType);
        instruction->InputId = inputId;
        return instruction;
    }

    MIRValue* MIREmitter::TextureObject(int textureSlotIndex)
    {
        MIRTextureObject* instruction = m_Graph->CreateValue<MIRTextureObject>();
        InitializeMIRInstruction(instruction, VK_TextureObject, MIRObjectType::GetTexture2D());
        instruction->TextureSlotIndex = textureSlotIndex;
        return instruction;
    }

    MIRValue* MIREmitter::TextureSample(MIRValue* texture, MIRValue* texCoord)
    {
        if (texture == nullptr || texture->IsPoison() || texCoord == nullptr || texCoord->IsPoison())
        {
            return Poison();
        }

        if (texture->Type == nullptr || texture->Type->AsObject() == nullptr)
        {
            EmitDiagnostic("TextureSample requires a Texture2D object.");
            return Poison();
        }

        MIRValue* uv = Cast(texCoord, MIRPrimitiveType::GetFloat2());
        if (uv == nullptr || uv->IsPoison())
        {
            return Poison();
        }

        MIRTextureRead* instruction = m_Graph->CreateValue<MIRTextureRead>();
        InitializeMIRInstruction(instruction, VK_TextureRead, MIRPrimitiveType::GetFloat4());
        instruction->Texture = texture;
        instruction->TexCoord = uv;
        return instruction;
    }

    MIRValue* MIREmitter::UniformScalar(int uniformSlotIndex, float defaultValue)
    {
        MIRUniformParameter* instruction = m_Graph->CreateValue<MIRUniformParameter>();
        InitializeMIRInstruction(instruction, VK_UniformParameter, MIRPrimitiveType::GetFloat());
        instruction->UniformSlotIndex = uniformSlotIndex;
        instruction->DefaultValue = defaultValue;
        return instruction;
    }

    ::minEngine::SetMaterialOutput* MIREmitter::SetMaterialOutput(MaterialProperty property, MIRValue* arg)
    {
        if (property < 0 || property >= MaterialPropCount)
        {
            EmitDiagnostic("SetMaterialOutput property is out of range.");
            return nullptr;
        }

        ::minEngine::SetMaterialOutput* instruction = m_Graph->CreateValue<::minEngine::SetMaterialOutput>();
        InitializeMIRInstruction(
            instruction,
            VK_SetMaterialOutput,
            arg != nullptr ? arg->Type : GetMaterialPropertyType(property));
        instruction->Arg = arg;
        instruction->Property = property;

        for (int stageIndex = 0; stageIndex < NumStages; ++stageIndex)
        {
            const ShaderStage stage = static_cast<ShaderStage>(stageIndex);
            if (MaterialPropertyEvaluatesInStage(property, stage))
            {
                m_Graph->AddOutput(stage, instruction);
            }
        }

        return instruction;
    }

}
