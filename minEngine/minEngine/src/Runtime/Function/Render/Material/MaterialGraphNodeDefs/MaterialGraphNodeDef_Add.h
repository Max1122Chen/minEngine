#pragma once
#include "Core.h"
#include "MaterialGraphNodeDef.h"
namespace minEngine
{
    class MaterialGraphNodeDef_Add final : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Add();

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;
    };
}