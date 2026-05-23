#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/Material/MaterialEdGraph.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <string>
#include <vector>

namespace minEngine
{
    class RHIShader;
    class Shader;
    class MaterialCompiler;
    class MaterialGraphNodeDef_TextureObject;
    class MaterialGraphNodeDef_ScalarParameter;

    struct MaterialTextureParameter
    {
        std::string ParameterName;
        int SlotIndex = 0;
        std::string ShaderSymbolName;
        std::shared_ptr<Texture2D> Value;
    };

    struct MaterialScalarParameter
    {
        std::string ParameterName;
        int SlotIndex = 0;
        std::string ShaderSymbolName;
        float Value = 0.0f;
    };

    ME_CLASS()
    class Material : public Asset
    {
        ME_GENERATED_BODY(Material)
        friend class MaterialCompiler;

    public:
        Material() = default;
        virtual ~Material() = default;

        bool Compile();
        void BindForDraw(RHIShader& shader) const;

        void SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture);
        void SetScalarParameter(const std::string& parameterName, float value);

        bool IsCompiledForDraw() const
        {
            return m_Shader != nullptr && m_Shader->IsValid() && !m_ParameterLayout.Parameters.empty();
        }

        Shader* GetShader() const { return m_Shader.get(); }

        MaterialEdGraph& GetGraph();
        const MaterialEdGraph& GetGraph() const;

        bool LinkNodeDefGraph();
        bool RebuildPinLinks();
        bool ValidateMaterialAsset(std::string* outError = nullptr) const;
        bool FinalizeGraphAfterLoad(std::string* outError = nullptr);

        ME_PROPERTY()
        MaterialShadingModel m_ShadingModel = MaterialShadingModel::Unlit;

        ME_PROPERTY()
        MaterialBlendMode m_BlendMode = MaterialBlendMode::Opaque;

        ME_PROPERTY(Instanced)
        std::shared_ptr<MaterialEdGraph> m_Graph;

        std::vector<MaterialCompileDiagnostic> m_LastCompileDiagnostics;
        MaterialShaderParameterLayout m_ParameterLayout;
        std::vector<MaterialTextureParameter> m_TextureParameters;
        std::vector<MaterialScalarParameter> m_ScalarParameters;

        bool IsTranslucent() const { return m_BlendMode == MaterialBlendMode::Translucent; }
        bool IsMasked() const { return m_BlendMode == MaterialBlendMode::Masked; }

    private:
        std::shared_ptr<Shader> m_Shader;

        bool CommitCompileResult(const MaterialCompileResult& result, const MaterialCompileContext& ctx);

        const MaterialGraphNodeDef_TextureObject* FindTextureNodeBySlot(int slotIndex) const;
        const MaterialGraphNodeDef_ScalarParameter* FindScalarNodeBySlot(int slotIndex) const;

        MaterialTextureParameter* FindTextureParameter(const std::string& parameterName);
        MaterialTextureParameter* FindTextureParameterBySlot(int slotIndex);
        MaterialScalarParameter* FindScalarParameter(const std::string& parameterName);
        MaterialScalarParameter* FindScalarParameterBySlot(int slotIndex);
    };
}

#include "Generated/Reflection/Material.gen.h"
