#pragma once
#include "Core.h"
#include "MaterialGraphNodeDef.h"

namespace minEngine
{
    class MaterialGraphNodeDef_MaterialOutput final : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_MaterialOutput();

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;
    };
}
