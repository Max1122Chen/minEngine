#pragma once
#include "Core.h"
#include "MaterialGraphNodeDef.h"

namespace minEngine
{
    class MaterialGraphNodeDef_Constant final : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_Constant(float value = 0.0f);

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;

    private:
        float m_X;
    };

    class MaterialGraphNodeDef_Constant2 final : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Constant2(float x = 0.0f, float y = 0.0f);

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;

    private:
        float m_X;
        float m_Y;
    };
}