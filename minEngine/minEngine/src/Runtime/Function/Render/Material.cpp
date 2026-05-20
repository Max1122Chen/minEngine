#include "Material.h"

#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "RHI/RHIShader.h"
#include "RHI/RHITexture.h"
#include "Texture.h"

namespace minEngine
{
    bool Material::Compile()
    {
        MaterialCompileContext ctx;
        ctx.RHI = RenderSystem::Get().GetRHI();
        return MaterialCompiler::Compile(*this, ctx);
    }

    const MaterialGraphNodeDef_TextureObject* Material::FindTextureNodeBySlot(int slotIndex) const
    {
        for (const MaterialEdGraphNode& graphNode : m_Graph.m_Nodes)
        {
            if (const MaterialGraphNodeDef_TextureObject* textureNode =
                    dynamic_cast<const MaterialGraphNodeDef_TextureObject*>(graphNode.GetDefinition()))
            {
                if (textureNode->TextureSlotIndex == slotIndex)
                {
                    return textureNode;
                }
            }
        }

        return nullptr;
    }

    const MaterialGraphNodeDef_ScalarParameter* Material::FindScalarNodeBySlot(int slotIndex) const
    {
        for (const MaterialEdGraphNode& graphNode : m_Graph.m_Nodes)
        {
            if (const MaterialGraphNodeDef_ScalarParameter* scalarNode =
                    dynamic_cast<const MaterialGraphNodeDef_ScalarParameter*>(graphNode.GetDefinition()))
            {
                if (scalarNode->UniformSlotIndex == slotIndex)
                {
                    return scalarNode;
                }
            }
        }

        return nullptr;
    }

    bool Material::CommitCompileResult(const MaterialCompileResult& result, const MaterialCompileContext& ctx)
    {
        if (ctx.RHI == nullptr)
        {
            m_LastCompileDiagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "Material::CommitCompileResult requires a valid RHI in MaterialCompileContext.",
            });
            return false;
        }

        if (!m_Shader)
        {
            m_Shader = std::make_shared<Shader>();
        }

        std::string compileError;
        if (!m_Shader->CompileFromSource(
                *ctx.RHI,
                result.FullVertexShader,
                result.FullFragmentShader,
                &compileError))
        {
            if (!compileError.empty())
            {
                m_LastCompileDiagnostics.push_back({
                    MaterialCompileDiagnostic::Error,
                    compileError,
                });
            }
            return false;
        }

        m_ParameterLayout = result.ParameterLayout;
        m_TextureParameters.clear();
        m_ScalarParameters.clear();

        for (const MaterialShaderParameterDesc& parameterDesc : m_ParameterLayout.Parameters)
        {
            if (parameterDesc.Type == MaterialShaderParameterType::Texture2D)
            {
                MaterialTextureParameter textureParameter;
                textureParameter.SlotIndex = parameterDesc.SlotIndex;
                textureParameter.ShaderSymbolName = parameterDesc.ShaderSymbolName;

                if (const MaterialGraphNodeDef_TextureObject* textureNode = FindTextureNodeBySlot(parameterDesc.SlotIndex))
                {
                    textureParameter.ParameterName = textureNode->ParameterName;
                    textureParameter.Value = textureNode->DefaultTexture;
                }

                if (!textureParameter.Value)
                {
                    textureParameter.Value = Texture2D::CreateSolidRGBA(*ctx.RHI, 255, 255, 255, 255);
                }

                m_TextureParameters.push_back(textureParameter);
            }
            else if (parameterDesc.Type == MaterialShaderParameterType::Scalar)
            {
                MaterialScalarParameter scalarParameter;
                scalarParameter.SlotIndex = parameterDesc.SlotIndex;
                scalarParameter.ShaderSymbolName = parameterDesc.ShaderSymbolName;

                if (const MaterialGraphNodeDef_ScalarParameter* scalarNode = FindScalarNodeBySlot(parameterDesc.SlotIndex))
                {
                    scalarParameter.ParameterName = scalarNode->ParameterName;
                    scalarParameter.Value = scalarNode->DefaultValue;
                }

                m_ScalarParameters.push_back(scalarParameter);
            }
        }

        return true;
    }

    MaterialTextureParameter* Material::FindTextureParameter(const std::string& parameterName)
    {
        for (MaterialTextureParameter& parameter : m_TextureParameters)
        {
            if (parameter.ParameterName == parameterName)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialTextureParameter* Material::FindTextureParameterBySlot(int slotIndex)
    {
        for (MaterialTextureParameter& parameter : m_TextureParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialScalarParameter* Material::FindScalarParameter(const std::string& parameterName)
    {
        for (MaterialScalarParameter& parameter : m_ScalarParameters)
        {
            if (parameter.ParameterName == parameterName)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    MaterialScalarParameter* Material::FindScalarParameterBySlot(int slotIndex)
    {
        for (MaterialScalarParameter& parameter : m_ScalarParameters)
        {
            if (parameter.SlotIndex == slotIndex)
            {
                return &parameter;
            }
        }

        return nullptr;
    }

    void Material::SetTextureParameter(const std::string& parameterName, std::shared_ptr<Texture2D> texture)
    {
        if (MaterialTextureParameter* parameter = FindTextureParameter(parameterName))
        {
            parameter->Value = std::move(texture);
        }
    }

    void Material::SetScalarParameter(const std::string& parameterName, float value)
    {
        if (MaterialScalarParameter* parameter = FindScalarParameter(parameterName))
        {
            parameter->Value = value;
        }
    }

    void Material::BindForDraw(RHIShader& shader) const
    {
        for (const MaterialTextureParameter& textureParameter : m_TextureParameters)
        {
            const std::shared_ptr<Texture2D>& texture = textureParameter.Value;
            if (!texture || texture->GetRHITexture() == nullptr)
            {
                continue;
            }

            const int textureUnit = textureParameter.SlotIndex;
            texture->GetRHITexture()->Bind(textureUnit);
            shader.UploadUniformInt(textureParameter.ShaderSymbolName, textureUnit);
        }

        for (const MaterialScalarParameter& scalarParameter : m_ScalarParameters)
        {
            shader.UploadUniformFloat(scalarParameter.ShaderSymbolName, scalarParameter.Value);
        }
    }
}
