#pragma once
#include "Core.h"
#include "MaterialIR.h"
#include "MaterialIRTypes.h"

namespace minEngine
{
    class MIRBuilder;
    class MIRGraph;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;
    class MaterialGraphNodeDefOutput;
    class Material;

    class MIREmitter
    {
        friend class MIRBuilder;
    public:
        MIRValue* Poison();
        MIRValue* ConstantBool(bool value);
        MIRValue* ConstantInt(int64_t value);
        MIRValue* ConstantFloat(float value);
        MIRValue* ConstantFloat3(float r, float g, float b);
        MIRValue* ConstantZero(const MIRPrimitiveType* scalarType);
        MIRValue* ConstantDefaultForProperty(MaterialProperty property);

        MIRValue* MakeDimensional(const MIRPrimitiveType* type, std::vector<MIRValue*> components);
        MIRValue* Vector3(MIRValue* x, MIRValue* y, MIRValue* z);

        const MIRValueType* TryGetCommonType(const MIRValueType* a, const MIRValueType* b) const;
        const MIRValueType* GetCommonType(const MIRValueType* a, const MIRValueType* b);
        MIRValue* Cast(MIRValue* value, const MIRValueType* targetType);
        MIRValue* Input(const MaterialGraphNodeDefInput* input);
        void Output(int outputIndex, MIRValue* value);
        void Output(const MaterialGraphNodeDefOutput* output, MIRValue* value);
        bool IsOutputConnected(int outputIndex) const;

        MIRValue* Operator(MIROperatorCode op, MIRValue* a, MIRValue* b = nullptr, MIRValue* c = nullptr);
        MIRValue* Negative(MIRValue* value);
        MIRValue* Not(MIRValue* value);
        MIRValue* Add(MIRValue* left, MIRValue* right);
        MIRValue* Subtract(MIRValue* left, MIRValue* right);
        MIRValue* Multiply(MIRValue* left, MIRValue* right);
        MIRValue* Divide(MIRValue* left, MIRValue* right);
        MIRValue* Max(MIRValue* left, MIRValue* right);
        MIRValue* Min(MIRValue* left, MIRValue* right);
        MIRValue* Select(MIRValue* condition, MIRValue* trueValue, MIRValue* falseValue);

        MIRValue* Branch(MIRValue* condition, MIRValue* trueValue, MIRValue* falseValue);

        // Extracts one channel; always emits MIRSubscript when Arg is a vector (for visible .x/.y/.z in GLSL).
        MIRValue* SubscriptChannel(MIRValue* value, int channelIndex);

        MIRValue* ExternalInput(MIRExternalInputId inputId);
        MIRValue* TextureObject(int textureSlotIndex);
        MIRValue* TextureSample(MIRValue* texture, MIRValue* texCoord);
        MIRValue* UniformScalar(int uniformSlotIndex, float defaultValue);

        struct SetMaterialOutput* SetMaterialOutput(MaterialProperty property, MIRValue* arg);

        void EmitDiagnostic(const std::string& message);

    private:
        static MIRValue* CastConstant(MIREmitter& emitter, MIRConstant* constant, MIRScalarKind sourceKind,
                                      MIRScalarKind targetKind);
        MIRValue* CastValueToPrimitiveType(MIRValue* value, const MIRPrimitiveType* targetPrimitiveType);
        MIRValue* Subscript(MIRValue* value, int index);
        MIRValue* EmitSubscriptInstruction(MIRValue* value, int index);
        MIRValue* EmitOperator(const MIRPrimitiveType* resultType, MIROperatorCode op, MIRValue* a, MIRValue* b, MIRValue* c);
        const MIRPrimitiveType* ValidateOperatorAndGetResultType(MIROperatorCode op, MIRValue*& a, MIRValue*& b, MIRValue*& c);
        MIRValue* TryFoldOperator(MIROperatorCode op, MIRValue* a, MIRValue* b, MIRValue* c, const MIRPrimitiveType* resultType);

        MIRBuilder* m_Builder = nullptr;
        MIRGraph* m_Graph = nullptr;
        MaterialGraphNodeDef* m_CurrentNodeDef = nullptr;
    };
}
