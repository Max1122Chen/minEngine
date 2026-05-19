#include "MaterialIRTest.h"

#include "Log/LogSystem.h"
#include "../MaterialCompiler/MaterialCompiler.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialPropertyUtil.h"
#include "../MaterialCompiler/MIRToGLSLTranslator.h"

#include <filesystem>
#include <fstream>
#include <string_view>

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

        bool Contains(std::string_view haystack, std::string_view needle)
        {
            return haystack.find(needle) != std::string_view::npos;
        }

        int CountOccurrences(std::string_view haystack, std::string_view needle)
        {
            int count = 0;
            size_t position = 0;
            while ((position = haystack.find(needle, position)) != std::string_view::npos)
            {
                ++count;
                position += needle.size();
            }
            return count;
        }

        bool AssertContains(const std::string& haystack, std::string_view needle, const char* label)
        {
            if (Contains(haystack, needle))
            {
                return true;
            }

            ME_CORE_ERROR("MaterialIR smoke: {} missing substring '{}'", label, needle);
            return false;
        }

        bool AssertAllContains(const std::string& haystack, std::initializer_list<std::pair<std::string_view, const char*>> needles)
        {
            bool allPassed = true;
            for (const auto& entry : needles)
            {
                if (!AssertContains(haystack, entry.first, entry.second))
                {
                    allPassed = false;
                }
            }
            return allPassed;
        }

        bool AssertTrue(bool condition, const char* label)
        {
            if (condition)
            {
                return true;
            }

            ME_CORE_ERROR("MaterialIR binding check failed: {}", label);
            return false;
        }

        void WriteTextArtifact(const std::filesystem::path& path, const std::string& contents)
        {
            std::error_code errorCode;
            std::filesystem::create_directories(path.parent_path(), errorCode);

            std::ofstream outputFile(path, std::ios::trunc);
            if (outputFile.is_open())
            {
                outputFile << contents;
            }
        }

        void WriteCompileArtifacts(const MaterialCompiledShader& result)
        {
            const std::filesystem::path outputDir = "Saved/Materials";
            WriteTextArtifact(outputDir / "IRDump.txt", result.IRDump);
            WriteTextArtifact(outputDir / "GeneratedVertex.glsl", result.FullVertexShader);
            WriteTextArtifact(outputDir / "GeneratedFragment.glsl", result.FullFragmentShader);
            WriteTextArtifact(outputDir / "FragmentStageBody.glsl", result.Stages[Stage_Fragment].Body);
            WriteTextArtifact(outputDir / "FragmentStagePreamble.glsl", result.Stages[Stage_Fragment].Preamble);
            WriteTextArtifact(outputDir / "VertexStageBody.glsl", result.Stages[Stage_Vertex].Body);
        }

        void LogCompiledShaders(const MaterialCompiledShader& result)
        {
            ME_CORE_INFO("======== MaterialIR generated vertex shader ========");
            ME_CORE_INFO("\n{}", result.FullVertexShader);
            ME_CORE_INFO("======== MaterialIR generated fragment shader ======");
            ME_CORE_INFO("\n{}", result.FullFragmentShader);
            ME_CORE_INFO("======== MaterialIR fragment stage body ============");
            ME_CORE_INFO("\n{}", result.Stages[Stage_Fragment].Body);
            ME_CORE_INFO("Artifacts: Saved/Materials/GeneratedVertex.glsl, GeneratedFragment.glsl, IRDump.txt");
        }

        bool VerifyPropertyBindingLayer(const MaterialEdGraph& graph, const MaterialGraphNodeDef_MaterialOutput& output)
        {
            bool passed = true;

            for (int propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
            {
                const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
                MaterialPropertyInputDescription description;
                passed = AssertTrue(GetMaterialPropertyInputDescription(property, description), "GetMaterialPropertyInputDescription")
                    && passed;
                passed = AssertTrue(description.InputName == GetMaterialPropertyName(property), "description InputName")
                    && passed;
                passed = AssertTrue(description.ExpectedType == GetMaterialPropertyType(property), "description ExpectedType")
                    && passed;

                const MaterialGraphNodeDefInput* pinByName = output.FindInputByName(description.InputName);
                passed = AssertTrue(pinByName != nullptr, "MaterialOutput FindInputByName") && passed;

                MaterialPropertyInputDescription resolved = description;
                passed = AssertTrue(graph.ResolveMaterialPropertyInput(property, resolved), "ResolveMaterialPropertyInput")
                    && passed;
                passed = AssertTrue(resolved.GraphInput == pinByName, "resolved GraphInput matches pin") && passed;
            }

            passed = AssertTrue(output.FindInputByName("NotAPin") == nullptr, "unknown pin returns null") && passed;

            const std::vector<MaterialGraphNodeDef*> outputNodes = graph.GetMaterialOutputNodeDefs();
            passed = AssertTrue(outputNodes.size() == 1, "single MaterialOutput in smoke graph") && passed;
            passed = AssertTrue(outputNodes[0]->IsMaterialOutputNode(), "IsMaterialOutputNode") && passed;

            MaterialPropertyInputDescription metallicDescription;
            passed = AssertTrue(graph.ResolveMaterialPropertyInput(MP_Metallic, metallicDescription), "Metallic resolve")
                && passed;
            passed = AssertTrue(
                metallicDescription.GraphInput != nullptr && metallicDescription.GraphInput->IsConnected(),
                "Metallic pin connected")
                && passed;

            MaterialPropertyInputDescription emissiveDescription;
            passed = AssertTrue(graph.ResolveMaterialPropertyInput(MP_Emissive, emissiveDescription), "Emissive resolve")
                && passed;
            passed = AssertTrue(
                emissiveDescription.GraphInput != nullptr && emissiveDescription.GraphInput->IsConnected(),
                "Emissive pin connected")
                && passed;
            passed = AssertTrue(metallicDescription.GraphInput != emissiveDescription.GraphInput, "distinct Metallic/Emissive pins")
                && passed;

            if (passed)
            {
                ME_CORE_INFO("MaterialIR property binding checks PASSED.");
            }

            return passed;
        }

        // Smoke graph (single integrated material):
        //   Albedo   <- TextureSample(RGB) <- TextureObject(0) + TextureCoordinate (ExternalInput TexCoord0)
        //   Metallic <- ScalarParameter(0.3)
        //   Emissive <- MakeFloat3(0.2, 0.8, 0.2)
        //   Roughness/Opacity <- defaults
        //
        // Expected shading (Unlit): FragColor.rgb = Albedo + Emissive, alpha = Opacity.
        // With default texture=1 and UV=(0,0): Albedo=(1,1,1) -> FragColor.rgb = (1.2, 0.8, 0.2).
        bool VerifySmokeCompileResult(const MaterialCompiledShader& result)
        {
            if (!result.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : result.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR smoke diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            bool passed = true;

            passed = AssertAllContains(result.IRDump, {
                { "; minEngine Material IR dump", "IR header" },
                { "SetMaterialOutput \"Albedo\"", "IR Albedo output" },
                { "ExternalInput", "IR texcoord input" },
                { "TextureRead", "IR texture sample" },
                { "UniformParameter", "IR scalar uniform" },
            }) && passed;

            passed = AssertAllContains(result.Stages[Stage_Fragment].Body, {
                { "FragmentMaterialInputs.Albedo =", "fragment body Albedo assign" },
                { "FragmentMaterialInputs.Metallic = u_ScalarParam0", "fragment body Metallic assign" },
                { "texture(u_Texture0, MaterialParameters.TexCoords[0])", "UE-style texcoord access" },
            }) && passed;

            passed = AssertAllContains(result.FullVertexShader, {
                { "} MaterialParameters;", "vertex MaterialParameters struct global" },
                { "MaterialParameters.TexCoords[0] = a_TexCoord", "vertex fills MaterialParameters" },
                { "v_MaterialTexCoord0", "vertex varying out" },
            }) && passed;

            passed = AssertTrue(
                result.FullVertexShader.find("} MaterialParameters;") < result.FullVertexShader.find("void main"),
                "vertex MaterialParameters struct must be outside main")
                && passed;

            passed = AssertAllContains(result.FullFragmentShader, {
                { "uniform sampler2D u_Texture0", "global texture uniform" },
                { "MaterialParameters.TexCoords[0] = v_MaterialTexCoord0", "fragment restores MaterialParameters" },
                { "FragColor = vec4(FragmentMaterialInputs.Albedo + FragmentMaterialInputs.Emissive, FragmentMaterialInputs.Opacity)", "unlit composite" },
            }) && passed;

            passed = AssertTrue(
                result.FullFragmentShader.find("uniform sampler2D") < result.FullFragmentShader.find("void main"),
                "uniforms must be declared outside main")
                && passed;

            passed = AssertTrue(
                result.FullFragmentShader.find("} MaterialParameters;") < result.FullFragmentShader.find("void main"),
                "fragment MaterialParameters struct must be outside main")
                && passed;

            passed = AssertTrue(!Contains(result.Stages[Stage_Fragment].Body, "v_TexCoord0"), "body must not use legacy v_TexCoord0")
                && passed;

            passed = AssertTrue(CountOccurrences(result.Stages[Stage_Fragment].Body, "FragmentMaterialInputs.Albedo =") == 1, "single Albedo assign")
                && passed;
            passed = AssertTrue(CountOccurrences(result.Stages[Stage_Fragment].Body, "FragmentMaterialInputs.Metallic =") == 1, "single Metallic assign")
                && passed;
            passed = AssertTrue(CountOccurrences(result.Stages[Stage_Fragment].Body, "FragmentMaterialInputs.Roughness =") == 1, "single Roughness assign")
                && passed;

            if (!passed)
            {
                LogCompiledShaders(result);
                ME_CORE_ERROR("MaterialIR smoke FAILED (see logged shaders above).");
                return false;
            }

            WriteCompileArtifacts(result);
            LogCompiledShaders(result);

            ME_CORE_INFO(
                "MaterialIR smoke PASSED.\n"
                "Graph: TextureSample->Albedo, Scalar(0.3)->Metallic, float3(0.2,0.8,0.2)->Emissive.\n"
                "If texture=1 at UV0: FragColor = vec4(1.2, 0.8, 0.2, 1).");
            return true;
        }

    }

    bool ShouldRunMaterialIRTestsOnly(int argc, char** argv)
    {
        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] != nullptr && std::string_view(argv[argIndex]) == "--material-ir-test")
            {
                return true;
            }
        }
        return false;
    }

    bool RunMaterialIRSmokeTests()
    {
        MaterialGraphNodeDef_TextureCoordinate texCoord;
        MaterialGraphNodeDef_TextureObject textureObject(0);
        MaterialGraphNodeDef_TextureSample textureSample;
        MaterialGraphNodeDef_ScalarParameter metallicScalar(0, 0.3f);
        MaterialGraphNodeDef_Constant emissiveR(0.2f);
        MaterialGraphNodeDef_Constant emissiveG(0.8f);
        MaterialGraphNodeDef_Constant emissiveB(0.2f);
        MaterialGraphNodeDef_MakeFloat3 emissiveColor;
        MaterialGraphNodeDef_MaterialOutput output;

        Connect(textureObject, 0, textureSample, 0);
        Connect(texCoord, 0, textureSample, 1);
        Connect(emissiveR, 0, emissiveColor, 0);
        Connect(emissiveG, 0, emissiveColor, 1);
        Connect(emissiveB, 0, emissiveColor, 2);
        Connect(textureSample, 1, output, 0);
        ConnectToProperty(emissiveColor, 0, output, MP_Emissive);
        ConnectToProperty(metallicScalar, 0, output, MP_Metallic);

        MaterialEdGraph graph;
        MaterialGraphNodeDef* nodeDefs[] = {
            &texCoord, &textureObject, &textureSample, &metallicScalar,
            &emissiveR, &emissiveG, &emissiveB, &emissiveColor, &output,
        };

        for (MaterialGraphNodeDef* nodeDef : nodeDefs)
        {
            MaterialEdGraphNode graphNode;
            graphNode.m_Definition = nodeDef;
            graph.m_Nodes.push_back(graphNode);
        }

        if (!VerifyPropertyBindingLayer(graph, output))
        {
            return false;
        }

        MIRToGLSLTranslator translator;
        MaterialCompileEnvironment env;
        env.ShadingMode = MaterialShadingMode::Unlit;
        if (!VerifySmokeCompileResult(MaterialCompiler::Compile(graph, translator, env)))
        {
            return false;
        }

        ME_CORE_INFO("MaterialIR all tests PASSED (binding + smoke compile).");
        return true;
    }
}
