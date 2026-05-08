#pragma once
#include "Core.h"
#include "MaterialGraphNodeDef.h"

#include <string>

namespace minEngine
{
    class MaterialGraphNodeDef_Texture2DParameter final : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_Texture2DParameter(const std::string& name);

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;

    private:
        std::string m_Name;
    };

    class MaterialGraphNodeDef_TextureSample final : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_TextureSample();

        MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) override;
    };
}
