#include "MaterialSmokeGraph.h"

#include "MaterialCompiler/MaterialCompiler.h"
#include "MaterialPropertyUtil.h"

namespace minEngine
{
    namespace
    {
        void Connect(MaterialGraphNodeDef& from, int fromOutputIndex, MaterialGraphNodeDef& to, int toInputIndex)
        {
            MaterialGraphNodeDefInput* input = to.GetInput(toInputIndex);
            if (input == nullptr)
            {
                return;
            }
            input->NodeDef = &from;
            input->OutputIndex = fromOutputIndex;
        }

        void ConnectToProperty(
            MaterialGraphNodeDef& from,
            int fromOutputIndex,
            MaterialGraphNodeDef_MaterialOutput& output,
            MaterialProperty property)
        {
            MaterialPropertyInputDescription description;
            if (!GetMaterialPropertyInputDescription(property, description))
            {
                return;
            }

            MaterialGraphNodeDefInput* input = output.FindInputByName(description.InputName);
            if (input == nullptr)
            {
                return;
            }

            input->NodeDef = &from;
            input->OutputIndex = fromOutputIndex;
        }
    }

    MaterialSmokeGraph::MaterialSmokeGraph()
        : m_TextureObject(0)
        , m_MetallicScalar(0, 0.3f)
        , m_EmissiveR(0.2f)
        , m_EmissiveG(0.8f)
        , m_EmissiveB(0.2f)
    {
        Connect(m_TextureObject, 0, m_TextureSample, 0);
        Connect(m_TexCoord, 0, m_TextureSample, 1);
        Connect(m_EmissiveR, 0, m_TintColor, 0);
        Connect(m_EmissiveG, 0, m_TintColor, 1);
        Connect(m_EmissiveB, 0, m_TintColor, 2);
        Connect(m_TextureSample, 1, m_AlbedoTintMultiply, 0);
        Connect(m_TintColor, 0, m_AlbedoTintMultiply, 1);
        ConnectToProperty(m_AlbedoTintMultiply, 0, m_Output, MP_Albedo);
        ConnectToProperty(m_MetallicScalar, 0, m_Output, MP_Metallic);

        MaterialGraphNodeDef* nodeDefs[] = {
            &m_TexCoord,
            &m_TextureObject,
            &m_TextureSample,
            &m_MetallicScalar,
            &m_EmissiveR,
            &m_EmissiveG,
            &m_EmissiveB,
            &m_TintColor,
            &m_AlbedoTintMultiply,
            &m_Output,
        };

        for (MaterialGraphNodeDef* nodeDef : nodeDefs)
        {
            MaterialEdGraphNode graphNode;
            graphNode.m_Definition = nodeDef;
            m_Graph.m_Nodes.push_back(graphNode);
        }
    }

    MaterialCompiledShader MaterialSmokeGraph::CompileUnlit() const
    {
        MaterialCompileEnvironment env;
        env.ShadingModel = MaterialShadingModel::Unlit;
        return MaterialCompiler::Compile(m_Graph, env);
    }
}
