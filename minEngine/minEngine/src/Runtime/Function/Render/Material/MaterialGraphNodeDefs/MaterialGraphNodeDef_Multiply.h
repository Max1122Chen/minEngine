#pragma once
#include "Core.h"
#include "MaterialGraphNodeDef.h"

namespace minEngine
{
    class MaterialGraphNodeDef_Multiply final : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Multiply();

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;
    };
}
