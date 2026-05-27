#include "MaterialIRTest.h"

#include "Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Serialization/JsonArchive.h"
#include "Serialization/Serializer.h"
#include "../MaterialCompiler/MaterialCompiler.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialCapability.h"
#include "../MaterialPropertyUtil.h"
#include "../MaterialValueType.h"
#include "../../Material.h"
#include "../MaterialTestGraph.h"
#include "Render/Environment/EngineIBLEnvironment.h"
#include "Render/Environment/EnvMapCapture.h"
#include "Render/OpenGL/OpenGLRHI.h"
#include "Render/OpenGL/OpenGLTexture.h"
#include "Render/Shader.h"
#include "Render/TextureCubeLoader.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace minEngine
{
    class MaterialIRTestObjectManagerScope
    {
    public:
        MaterialIRTestObjectManagerScope()
        {
            ObjectManager::SetInstance(&m_Manager);
            m_Manager.Initialize();
        }

        ~MaterialIRTestObjectManagerScope()
        {
            m_Manager.Shutdown();
            ObjectManager::SetInstance(nullptr);
        }

    private:
        ObjectManager m_Manager;
    };

    namespace
    {
        std::string g_MaterialIRTestEngineDefaultAssetsRoot;

        bool EnsureReflectionReadyForMaterialIRTest()
        {
            Reflection::ReflectionSystem& reflection = Reflection::ReflectionSystem::Get();
            if (reflection.IsReady())
            {
                return true;
            }

            if (!reflection.FinalizeReflection())
            {
                const std::vector<std::string>& reflectionErrors = reflection.GetLastErrors();
                for (const std::string& error : reflectionErrors)
                {
                    ME_CORE_ERROR("{}", error);
                }
                return false;
            }

            reflection.ClearErrors();
            return true;
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

        void WriteCompileArtifacts(const MaterialCompileResult& result)
        {
            const std::filesystem::path outputDir = "Saved/Materials";
            WriteTextArtifact(outputDir / "IRDump.txt", result.IRDump);
            WriteTextArtifact(outputDir / "GeneratedVertex.glsl", result.FullVertexShader);
            WriteTextArtifact(outputDir / "GeneratedFragment.glsl", result.FullFragmentShader);
            WriteTextArtifact(outputDir / "FragmentStageBody.glsl", result.Stages[Stage_Fragment].Body);
            WriteTextArtifact(outputDir / "FragmentStagePreamble.glsl", result.Stages[Stage_Fragment].Preamble);
            WriteTextArtifact(outputDir / "VertexStageBody.glsl", result.Stages[Stage_Vertex].Body);
        }

        class ScopedShaderCompileGlContext
        {
        public:
            ScopedShaderCompileGlContext()
            {
                if (glfwGetCurrentContext() != nullptr)
                {
                    return;
                }

                if (!glfwInit())
                {
                    return;
                }

                m_OwnsGlfw = true;
                glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                m_Window = glfwCreateWindow(1, 1, "MaterialIRGpuCompile", nullptr, nullptr);
                if (m_Window == nullptr)
                {
                    return;
                }

                glfwMakeContextCurrent(m_Window);
                if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
                {
                    m_Window = nullptr;
                }
            }

            ~ScopedShaderCompileGlContext()
            {
                if (m_Window != nullptr)
                {
                    glfwDestroyWindow(m_Window);
                    m_Window = nullptr;
                }

                if (m_OwnsGlfw)
                {
                    glfwTerminate();
                }
            }

            bool IsReady() const
            {
                return glfwGetCurrentContext() != nullptr;
            }

        private:
            GLFWwindow* m_Window = nullptr;
            bool m_OwnsGlfw = false;
        };

        bool VerifySmokeGpuCompile(const MaterialCompileResult& result)
        {
            ScopedShaderCompileGlContext glContext;
            if (!glContext.IsReady())
            {
                ME_CORE_ERROR("MaterialIR smoke: failed to create OpenGL context for GPU shader compile test.");
                return false;
            }

            std::string compileError;
            if (!Shader::TryCompileSourcesOnGpu(result.FullVertexShader, result.FullFragmentShader, &compileError))
            {
                ME_CORE_ERROR("MaterialIR smoke: GPU shader compile failed.\n{}", compileError);
                return false;
            }

            ME_CORE_INFO("MaterialIR smoke: GPU vertex/fragment compile + link PASSED.");
            return true;
        }

        void LogCompiledShaders(const MaterialCompileResult& result)
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

            MaterialPropertyInputDescription albedoDescription;
            passed = AssertTrue(graph.ResolveMaterialPropertyInput(MP_Albedo, albedoDescription), "Albedo resolve")
                && passed;
            passed = AssertTrue(
                albedoDescription.GraphInput != nullptr && albedoDescription.GraphInput->IsConnected(),
                "Albedo pin connected")
                && passed;
            passed = AssertTrue(metallicDescription.GraphInput != albedoDescription.GraphInput, "distinct Metallic/Albedo pins")
                && passed;

            if (passed)
            {
                ME_CORE_INFO("MaterialIR property binding checks PASSED.");
            }

            return passed;
        }

        // Smoke graph:
        //   Albedo <- Multiply(TextureSample.rgb, tint vec3(0.2, 0.8, 0.2))
        //   Metallic <- ScalarParameter(0.3)
        //
        // Expected Unlit: FragColor.rgb = Albedo (tint baked in graph).
        // White texture at UV0: rgb = (0.2, 0.8, 0.2).
        bool VerifySmokeCompileResult(const MaterialCompileResult& result)
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

            passed = AssertTrue(result.ParameterLayout.Parameters.size() == 1, "unlit parameter layout entry count")
                && passed;

            bool foundTextureLayout = false;
            for (const MaterialShaderParameterDesc& parameterDesc : result.ParameterLayout.Parameters)
            {
                if (parameterDesc.Type == MaterialShaderParameterType::Texture2D
                    && parameterDesc.SlotIndex == 0
                    && parameterDesc.ShaderSymbolName == "u_Texture0")
                {
                    foundTextureLayout = true;
                }
            }

            passed = AssertTrue(foundTextureLayout, "layout u_Texture0") && passed;

            passed = AssertAllContains(result.IRDump, {
                { "; minEngine Material IR dump", "IR header" },
                { "SetMaterialOutput \"Albedo\"", "IR Albedo output" },
                { "ExternalInput", "IR texcoord input" },
                { "TextureRead", "IR texture sample" },
                { "Multiply", "IR multiply node" },
            }) && passed;

            passed = AssertAllContains(result.Stages[Stage_Fragment].Body, {
                { "FragmentMaterialInputs.Albedo =", "fragment body Albedo assign" },
                { "texture(u_Texture0, MaterialParameters.TexCoords[0])", "UE-style texcoord access" },
            }) && passed;

            passed = AssertTrue(
                result.Stages[Stage_Fragment].Body.find("FragmentMaterialInputs.Normal") == std::string::npos,
                "unlit body must not assign Normal")
                && passed;
            passed = AssertTrue(
                result.Stages[Stage_Fragment].Body.find("FragmentMaterialInputs.Metallic") == std::string::npos,
                "unlit body must not assign Metallic")
                && passed;

            passed = AssertAllContains(result.FullVertexShader, {
                { "layout (std140) uniform PerFrameData", "vertex PerFrameData UBO" },
                { "uniform mat4 u_Model", "vertex u_Model uniform" },
                { "ViewProj * u_Model * vec4(a_Position, 1.0)", "vertex standard transform" },
                { "} MaterialParameters;", "vertex MaterialParameters struct global" },
                { "MaterialParameters.TexCoords[0] = a_TexCoord", "vertex fills MaterialParameters" },
                { "v_MaterialTexCoord0", "vertex varying out" },
            }) && passed;

            passed = AssertTrue(
                result.FullVertexShader.find("a_Normal") == std::string::npos,
                "unlit vertex must not require a_Normal")
                && passed;

            passed = AssertTrue(
                result.FullVertexShader.find("} MaterialParameters;") < result.FullVertexShader.find("void main"),
                "vertex MaterialParameters struct must be outside main")
                && passed;

            passed = AssertAllContains(result.FullFragmentShader, {
                { "uniform sampler2D u_Texture0", "global texture uniform" },
                { "MaterialParameters.TexCoords[0] = v_MaterialTexCoord0", "fragment restores MaterialParameters" },
                { "FragmentMaterialInputs.Albedo + FragmentMaterialInputs.Emissive", "unlit self-lit composite" },
            }) && passed;

            passed = AssertTrue(
                result.FullFragmentShader.find("u_DirLightShadowMap") == std::string::npos,
                "unlit fragment must not reference shadow samplers")
                && passed;
            passed = AssertTrue(
                result.FullFragmentShader.find("LightsData") == std::string::npos,
                "unlit fragment must not reference scene lights UBO")
                && passed;

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
            passed = AssertTrue(
                result.FullFragmentShader.find("float Metallic") == std::string::npos,
                "unlit struct must not declare Metallic")
                && passed;

            if (!passed)
            {
                LogCompiledShaders(result);
                ME_CORE_ERROR("MaterialIR smoke FAILED (see logged shaders above).");
                return false;
            }

            passed = VerifySmokeGpuCompile(result) && passed;

            if (!passed)
            {
                LogCompiledShaders(result);
                ME_CORE_ERROR("MaterialIR smoke FAILED during GPU compile.");
                return false;
            }

            WriteCompileArtifacts(result);
            LogCompiledShaders(result);

            ME_CORE_INFO(
                "MaterialIR smoke PASSED.\n"
                "Graph: Albedo = TextureSample * tint(0.2,0.8,0.2), Metallic = 0.3.\n"
                "Unlit: FragColor = Albedo + Emissive. If texture=1 at UV0: rgb = (0.2, 0.8, 0.2).");
            return true;
        }

        bool VerifyConstant3ToNormalBlinnPhong(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& constant3Node = graph.AddNode<MaterialGraphNodeDef_Constant3>();
            MaterialGraphNodeDef_Constant3* constant3 =
                static_cast<MaterialGraphNodeDef_Constant3*>(constant3Node.GetNodeDef());
            constant3->R = 0.0f;
            constant3->G = 1.0f;
            constant3->B = 0.0f;

            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();
            const MaterialShadingModel shadingModel = MaterialShadingModel::BlinnPhong;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            if (!graph.ConnectToMaterialProperty(
                    constant3Node,
                    0,
                    outputNode,
                    MP_Normal,
                    shadingModel,
                    blendMode))
            {
                ME_CORE_ERROR("MaterialIR Constant3→Normal: failed to connect Normal pin.");
                return false;
            }

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR Constant3→Normal diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            bool passed = AssertAllContains(compiled.FullFragmentShader, {
                { "BuildWorldNormalFromTangentSpace", "Constant3→Normal uses TBN" },
            });
            passed = AssertAllContains(compiled.Stages[Stage_Fragment].Body, {
                { "FragmentMaterialInputs.Normal =", "Constant3→Normal body assign" },
                { "vec3(", "Constant3 vector literal" },
            }) && passed;
            passed = AssertTrue(
                compiled.Stages[Stage_Fragment].Body.find("1.000000") != std::string::npos
                    || compiled.Stages[Stage_Fragment].Body.find("1.0") != std::string::npos,
                "Constant3 green channel in Normal")
                && passed;

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR Constant3→Normal compile content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR Constant3→Normal GPU compile FAILED.");
                return false;
            }

            ME_CORE_INFO("MaterialIR Constant3→Normal BlinnPhong: compile + GPU link PASSED.");
            return true;
        }

        bool VerifyIfThenElseAlbedoBlinnPhong(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& conditionNode = graph.AddNode<MaterialGraphNodeDef_ScalarParameter>();
            MaterialGraphNodeDef_ScalarParameter* conditionScalar =
                static_cast<MaterialGraphNodeDef_ScalarParameter*>(conditionNode.GetNodeDef());
            conditionScalar->ParameterName = "BranchCondition";
            conditionScalar->UniformSlotIndex = 0;
            conditionScalar->DefaultValue = 1.0f;

            MaterialEdGraphNode& trueColorNode = graph.AddNode<MaterialGraphNodeDef_Constant3>();
            MaterialGraphNodeDef_Constant3* trueColor =
                static_cast<MaterialGraphNodeDef_Constant3*>(trueColorNode.GetNodeDef());
            trueColor->R = 1.0f;
            trueColor->G = 0.0f;
            trueColor->B = 0.0f;

            MaterialEdGraphNode& falseColorNode = graph.AddNode<MaterialGraphNodeDef_Constant3>();
            MaterialGraphNodeDef_Constant3* falseColor =
                static_cast<MaterialGraphNodeDef_Constant3*>(falseColorNode.GetNodeDef());
            falseColor->R = 0.0f;
            falseColor->G = 1.0f;
            falseColor->B = 0.0f;

            MaterialEdGraphNode& branchNode = graph.AddNode<MaterialGraphNodeDef_IfThenElse>();
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            const MaterialShadingModel shadingModel = MaterialShadingModel::BlinnPhong;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            graph.ConnectPins(conditionNode, 0, branchNode, 0, shadingModel, blendMode);
            graph.ConnectPins(trueColorNode, 0, branchNode, 1, shadingModel, blendMode);
            graph.ConnectPins(falseColorNode, 0, branchNode, 2, shadingModel, blendMode);
            if (!graph.ConnectToMaterialProperty(branchNode, 0, outputNode, MP_Albedo, shadingModel, blendMode))
            {
                ME_CORE_ERROR("MaterialIR IfThenElse: failed to connect Albedo.");
                return false;
            }

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR IfThenElse diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            bool passed = AssertAllContains(compiled.Stages[Stage_Fragment].Body, {
                { "if (", "IfThenElse emits dynamic branch in GLSL" },
                { "FragmentMaterialInputs.Albedo =", "branch drives Albedo" },
                { "u_ScalarParam0", "branch condition uses scalar uniform" },
            });
            passed = AssertAllContains(compiled.IRDump, {
                { "Branch", "IfThenElse lowers to MIR Branch" },
            }) && passed;

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR IfThenElse content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR IfThenElse GPU compile FAILED.");
                return false;
            }

            ME_CORE_INFO("MaterialIR IfThenElse→Albedo BlinnPhong: compile + GPU link PASSED.");
            return true;
        }

        bool VerifyTextureSampleSharedByTwoOutputs(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
            MaterialEdGraphNode& texObject = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            MaterialEdGraphNode& texSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            MaterialEdGraphNode& texCoord = *graph.m_Nodes[0];
            const MaterialShadingModel shadingModel = MaterialShadingModel::BlinnPhong;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            graph.ConnectPins(texObject, 0, texSample, 0, shadingModel, blendMode);
            graph.ConnectPins(texCoord, 0, texSample, 1, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(texSample, 1, outputNode, MP_Albedo, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(texSample, 1, outputNode, MP_Emissive, shadingModel, blendMode);

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR texture multi-use diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            const int textureCallCount = CountOccurrences(compiled.Stages[Stage_Fragment].Body, "texture(");
            bool passed = AssertTrue(textureCallCount == 1, "single texture() for dual property use")
                && AssertAllContains(compiled.Stages[Stage_Fragment].Body, {
                    { "FragmentMaterialInputs.Albedo =", "shared sample -> Albedo" },
                    { "FragmentMaterialInputs.Emissive =", "shared sample -> Emissive" },
                });

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR texture multi-use content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                ME_CORE_ERROR("MaterialIR texture multi-use GPU compile FAILED.");
                return false;
            }

            ME_CORE_INFO("MaterialIR TextureSample dual-output: compile + GPU link PASSED.");
            return true;
        }

        bool VerifyDivideByZeroPoisonDiagnostic(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& numerator = graph.AddNode<MaterialGraphNodeDef_Constant>();
            static_cast<MaterialGraphNodeDef_Constant*>(numerator.GetNodeDef())->Value = 1.0f;
            MaterialEdGraphNode& denominator = graph.AddNode<MaterialGraphNodeDef_Constant>();
            static_cast<MaterialGraphNodeDef_Constant*>(denominator.GetNodeDef())->Value = 0.0f;
            MaterialEdGraphNode& divideNode = graph.AddNode<MaterialGraphNodeDef_Divide>();
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            const MaterialShadingModel shadingModel = MaterialShadingModel::Unlit;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            graph.ConnectPins(numerator, 0, divideNode, 0, shadingModel, blendMode);
            graph.ConnectPins(denominator, 0, divideNode, 1, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(divideNode, 0, outputNode, MP_Albedo, shadingModel, blendMode);

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (compiled.Succeeded)
            {
                ME_CORE_ERROR("MaterialIR divide-by-zero: expected compile failure.");
                return false;
            }

            bool foundDiagnostic = false;
            for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
            {
                if (diagnostic.Message.find("Divide by zero") != std::string::npos
                    || diagnostic.Message.find("Invalid value for material property") != std::string::npos)
                {
                    foundDiagnostic = true;
                    break;
                }
            }

            if (!AssertTrue(foundDiagnostic, "divide-by-zero or poison property diagnostic"))
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR divide-by-zero diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            ME_CORE_INFO("MaterialIR divide-by-zero poison diagnostic PASSED.");
            return true;
        }

        bool VerifyFragmentStructMatchesCapability(
            const MaterialCompileResult& compiled,
            MaterialShadingModel shadingModel,
            MaterialBlendMode blendMode)
        {
            const std::vector<MaterialProperty> emitted =
                MaterialCapabilityUtil::GetFragmentPropertiesEmittedAtCompile(shadingModel, blendMode);

            bool passed = true;
            for (MaterialProperty property : emitted)
            {
                const std::string fieldName = GetMaterialPropertyName(property);
                const MIRPrimitiveType* propertyType = GetMaterialPropertyType(property);
                std::string glslType = propertyType != nullptr && propertyType->IsVector()
                    ? "vec" + std::to_string(propertyType->NumRows) + " "
                    : "float ";
                glslType += fieldName;

                passed = AssertTrue(
                    compiled.FullFragmentShader.find(glslType) != std::string::npos,
                    ("fragment struct field " + fieldName).c_str())
                    && passed;
            }

            if (shadingModel == MaterialShadingModel::Unlit)
            {
                passed = AssertTrue(
                    compiled.FullFragmentShader.find("float Metallic") == std::string::npos,
                    "unlit struct must not declare Metallic")
                    && passed;
                passed = AssertTrue(
                    compiled.FullFragmentShader.find("vec3 Normal") == std::string::npos,
                    "unlit struct must not declare Normal")
                    && passed;
                passed = AssertTrue(
                    compiled.FullFragmentShader.find("float AO") == std::string::npos,
                    "unlit struct must not declare AO")
                    && passed;
            }
            else if (shadingModel == MaterialShadingModel::BlinnPhong)
            {
                passed = AssertAllContains(compiled.FullFragmentShader, {
                    { "vec3 Normal", "BlinnPhong struct Normal" },
                    { "float AO", "BlinnPhong struct AO" },
                    { "float Metallic", "BlinnPhong struct Metallic" },
                    { "float Roughness", "BlinnPhong struct Roughness" },
                }) && passed;
            }

            if (passed)
            {
                ME_CORE_INFO(
                    "MaterialIR capability struct check PASSED (shadingModel={}, emitted fields={}).",
                    static_cast<int>(shadingModel),
                    emitted.size());
            }

            return passed;
        }

        bool VerifyTranslucentUnlitCompile(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& opacityConstant = graph.AddNode<MaterialGraphNodeDef_Constant>();
            static_cast<MaterialGraphNodeDef_Constant*>(opacityConstant.GetNodeDef())->Value = 0.5f;
            MaterialEdGraphNode& albedoConstant = graph.AddNode<MaterialGraphNodeDef_Constant3>();
            MaterialGraphNodeDef_Constant3* albedo =
                static_cast<MaterialGraphNodeDef_Constant3*>(albedoConstant.GetNodeDef());
            albedo->R = 1.0f;
            albedo->G = 0.0f;
            albedo->B = 0.0f;
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            const MaterialShadingModel shadingModel = MaterialShadingModel::Unlit;
            const MaterialBlendMode blendMode = MaterialBlendMode::Translucent;

            graph.ConnectToMaterialProperty(
                albedoConstant, 0, outputNode, MP_Albedo, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                opacityConstant, 0, outputNode, MP_Opacity, shadingModel, blendMode);

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR Translucent diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            bool passed = AssertTrue(
                compiled.FullFragmentShader.find("discard") == std::string::npos,
                "Translucent fragment must not contain discard")
                && AssertAllContains(compiled.FullFragmentShader, {
                    { "FragmentMaterialInputs.Opacity", "Translucent FragColor alpha" },
                    { "FragColor = vec4(", "Translucent FragColor assignment" },
                })
                && VerifyFragmentStructMatchesCapability(compiled, shadingModel, blendMode);

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR Translucent Unlit compile content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR Translucent Unlit GPU compile FAILED.");
                return false;
            }

            Material materialNoOpacity;
            materialNoOpacity.m_Graph = NewObject<MaterialEdGraph>("", &materialNoOpacity);
            MaterialEdGraph& graphNoOpacity = *materialNoOpacity.m_Graph;
            MaterialEdGraphNode& outputOnly = graphNoOpacity.AddNode<MaterialGraphNodeDef_MaterialOutput>();
            const MaterialCompileResult warnCompiled = MaterialCompiler::CompileForDiagnostics(
                graphNoOpacity,
                shadingModel,
                blendMode,
                ctx);
            if (!warnCompiled.Succeeded)
            {
                ME_CORE_ERROR("MaterialIR Translucent empty graph: expected successful compile.");
                return false;
            }

            bool foundOpacityWarning = false;
            for (const MaterialCompileDiagnostic& diagnostic : warnCompiled.Diagnostics)
            {
                if (diagnostic.Message.find("Translucent material: Opacity is not connected") != std::string::npos)
                {
                    foundOpacityWarning = true;
                    break;
                }
            }

            if (!AssertTrue(foundOpacityWarning, "Translucent unconnected Opacity compile warning"))
            {
                (void)outputOnly;
                return false;
            }

            ME_CORE_INFO("MaterialIR Translucent Unlit: compile + GPU link + opacity warning PASSED.");
            return true;
        }

        bool VerifyTextureCubeRHICreation()
        {
            ScopedShaderCompileGlContext glContext;
            if (!glContext.IsReady())
            {
                ME_CORE_ERROR("MaterialIR TextureCube: failed to create OpenGL context.");
                return false;
            }

            const std::array<uint8_t, 4> faceColors[6] = {
                std::array<uint8_t, 4>{ 255, 32, 32, 255 },
                std::array<uint8_t, 4>{ 32, 255, 32, 255 },
                std::array<uint8_t, 4>{ 32, 64, 255, 255 },
                std::array<uint8_t, 4>{ 255, 220, 32, 255 },
                std::array<uint8_t, 4>{ 220, 32, 255, 255 },
                std::array<uint8_t, 4>{ 32, 255, 220, 255 },
            };

            OpenGLRHI rhi;
            std::string error;
            std::shared_ptr<TextureCube> cube =
                TextureCubeLoader::CreateSolidColorCube(rhi, 4, faceColors, &error);
            if (!cube || cube->GetRHITexture() == nullptr || cube->GetRHITexture()->GetID() == 0)
            {
                ME_CORE_ERROR("MaterialIR TextureCube: CreateSolidColorCube failed ({})", error);
                return false;
            }

            const uint32_t textureId = cube->GetRHITexture()->GetID();
            if (!AssertTrue(textureId != 0, "cubemap GL texture id non-zero"))
            {
                return false;
            }

            if (!AssertTrue(cube->GetSize() == 4, "cubemap face size"))
            {
                return false;
            }

            ME_CORE_INFO("MaterialIR TextureCube: RHI upload PASSED (id={}).", textureId);
            return true;
        }

        bool VerifyPBRWorkflow(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& texCoord = graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
            MaterialEdGraphNode& albedoTex = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            static_cast<MaterialGraphNodeDef_TextureObject*>(albedoTex.GetNodeDef())->TextureSlotIndex = 0;
            MaterialEdGraphNode& albedoSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();
            MaterialEdGraphNode& roughTex = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            static_cast<MaterialGraphNodeDef_TextureObject*>(roughTex.GetNodeDef())->TextureSlotIndex = 1;
            MaterialEdGraphNode& roughSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();
            MaterialEdGraphNode& metallicConst = graph.AddNode<MaterialGraphNodeDef_Constant>();
            static_cast<MaterialGraphNodeDef_Constant*>(metallicConst.GetNodeDef())->Value = 0.0f;
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            const MaterialShadingModel shadingModel = MaterialShadingModel::PBR;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            graph.ConnectPins(albedoTex, 0, albedoSample, 0, shadingModel, blendMode);
            graph.ConnectPins(texCoord, 0, albedoSample, 1, shadingModel, blendMode);
            graph.ConnectPins(roughTex, 0, roughSample, 0, shadingModel, blendMode);
            graph.ConnectPins(texCoord, 0, roughSample, 1, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                albedoSample, 1, outputNode, MP_Albedo, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                roughSample, 2, outputNode, MP_Roughness, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                metallicConst, 0, outputNode, MP_Metallic, shadingModel, blendMode);

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR PBR workflow diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            bool passed = AssertAllContains(compiled.FullFragmentShader, {
                    { "MaterialPBRDistributionGGX", "PBR GGX distribution" },
                    { "CalcDirLightPBR", "PBR directional light" },
                    { "BuildWorldNormalFromTangentSpace", "PBR uses TBN" },
                    { "FragmentMaterialInputs.Roughness", "PBR roughness input" },
                    { "CalcIndirectPBR", "PBR split-sum IBL" },
                    { "MaterialPBRIntegrateBrdfApprox", "PBR IBL BRDF fallback" },
                    { "u_EnvIntensity", "PBR IBL intensity uniform" },
                    { "u_Texture1", "roughness texture slot 1" },
                    { "u_EnvIrradianceMap", "PBR IBL irradiance sampler" },
                    { "u_EnvPrefilterMap", "PBR IBL prefilter sampler" },
                    { "u_EnvBrdfLUT", "PBR IBL BRDF LUT sampler" },
                })
                && AssertTrue(
                    compiled.FullFragmentShader.find("texture(u_Texture1") != std::string::npos,
                    "PBR samples roughness texture")
                && AssertTrue(
                    compiled.FullFragmentShader.find("texture(irradianceMap") != std::string::npos
                        || compiled.FullFragmentShader.find("texture( irradianceMap") != std::string::npos,
                    "PBR IBL samples irradiance cubemap")
                && AssertTrue(
                    compiled.FullFragmentShader.find("textureLod(prefilterMap") != std::string::npos,
                    "PBR IBL samples prefilter cubemap with LOD")
                && AssertTrue(
                    compiled.FullFragmentShader.find("texture(brdfLUT") != std::string::npos,
                    "PBR IBL samples BRDF LUT");

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR PBR workflow compile content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR PBR workflow GPU compile FAILED.");
                return false;
            }

            ME_CORE_INFO("MaterialIR PBR workflow (IBL): compile + GPU link PASSED.");
            return true;
        }

        bool VerifyEngineIBLEnvironmentInit()
        {
            ScopedShaderCompileGlContext glContext;
            if (!glContext.IsReady())
            {
                ME_CORE_ERROR("MaterialIR IBL environment: failed to create OpenGL context.");
                return false;
            }

            const std::string& assetsRoot = GetMaterialIRTestEngineDefaultAssetsRoot();
            if (assetsRoot.empty())
            {
                ME_CORE_WARN("MaterialIR IBL environment: skip (no EngineDefaultAssetsRoot).");
                return true;
            }

            OpenGLRHI rhi;
            EngineIBLEnvironment ibl;
            ibl.Initialize(&rhi, assetsRoot);

            if (!AssertTrue(ibl.HasIrradiance(), "IBL irradiance cubemap loaded"))
            {
                return false;
            }

            if (!AssertTrue(ibl.GetBrdfLUT() != nullptr, "IBL BRDF LUT ready"))
            {
                return false;
            }

            if (std::filesystem::is_regular_file(
                    std::filesystem::path(assetsRoot) / "Textures/IBL/brdf_lut.png"))
            {
                ME_CORE_INFO("MaterialIR IBL environment: brdf_lut.png present on disk.");
            }

            ME_CORE_INFO("MaterialIR IBL environment: irradiance + BRDF LUT init PASSED.");
            return true;
        }

        bool VerifyIBLEnvironmentFallbackChain()
        {
            ScopedShaderCompileGlContext glContext;
            if (!glContext.IsReady())
            {
                return false;
            }

            OpenGLRHI rhi;
            EngineIBLEnvironment ibl;
            ibl.Initialize(&rhi, "__minengine_ibl_missing_root__");

            if (!AssertTrue(ibl.HasIrradiance(), "IBL fallback still provides a cubemap"))
            {
                return false;
            }

            if (!AssertTrue(ibl.GetBrdfLUT() != nullptr, "IBL fallback still provides BRDF LUT"))
            {
                return false;
            }

            ME_CORE_INFO("MaterialIR IBL environment: missing-assets fallback chain PASSED.");
            return true;
        }

        bool HasIrradianceFacesOnDisk(const std::string& iblDirectory)
        {
            return std::filesystem::is_regular_file(
                std::filesystem::path(iblDirectory) / "irradiance_posx.png");
        }

        bool HasPrefilterFacesOnDisk(const std::string& iblDirectory)
        {
            return std::filesystem::is_regular_file(
                std::filesystem::path(iblDirectory) / "prefilter_posx.png");
        }

        bool HasHdrInIblDirectory(const std::string& iblDirectory)
        {
            if (!std::filesystem::is_directory(iblDirectory))
            {
                return false;
            }

            for (const std::filesystem::directory_entry& entry :
                 std::filesystem::directory_iterator(iblDirectory))
            {
                if (entry.is_regular_file() && ImageLoader::IsHdrPath(entry.path().string()))
                {
                    return true;
                }
            }
            return false;
        }

        bool VerifyIBLGpuConvolutionAndPrefilter()
        {
            ScopedShaderCompileGlContext glContext;
            if (!glContext.IsReady())
            {
                return false;
            }

            const std::string& assetsRoot = GetMaterialIRTestEngineDefaultAssetsRoot();
            if (assetsRoot.empty())
            {
                ME_CORE_WARN("MaterialIR IBL GPU passes: skip (no EngineDefaultAssetsRoot).");
                return true;
            }

            const std::filesystem::path iblDirectory =
                std::filesystem::path(assetsRoot) / "Textures/IBL";
            if (HasIrradianceFacesOnDisk(iblDirectory.string()) ||
                HasPrefilterFacesOnDisk(iblDirectory.string()))
            {
                ME_CORE_INFO("MaterialIR IBL GPU passes: skip (offline cubemap faces on disk).");
                return true;
            }

            if (!HasHdrInIblDirectory(iblDirectory.string()))
            {
                ME_CORE_WARN("MaterialIR IBL GPU passes: skip (no HDR in IBL folder).");
                return true;
            }

            OpenGLRHI rhi;
            EngineIBLEnvironment ibl;
            ibl.Initialize(&rhi, assetsRoot);

            if (!AssertTrue(ibl.HasEnvironment(), "IBL environment cubemap ready"))
            {
                return false;
            }

            if (!AssertTrue(ibl.HasIrradiance(), "IBL irradiance cubemap ready"))
            {
                return false;
            }

            if (!AssertTrue(ibl.HasPrefilter(), "IBL prefilter cubemap ready"))
            {
                return false;
            }

            const TextureCube* environment = ibl.GetEnvironment();
            const TextureCube* irradiance = ibl.GetIrradiance();
            const TextureCube* prefilter = ibl.GetPrefilter();
            if (!AssertTrue(
                    environment != nullptr && irradiance != nullptr && prefilter != nullptr,
                    "IBL cubemap pointers"))
            {
                return false;
            }

            if (!AssertTrue(
                    environment->GetSize() == EnvMapCapture::kDefaultCubeFaceSize,
                    "environment cubemap face size"))
            {
                return false;
            }

            if (!AssertTrue(
                    irradiance->GetSize() == EnvMapCapture::kDefaultIrradianceFaceSize,
                    "convolved irradiance face size"))
            {
                return false;
            }

            if (!AssertTrue(
                    prefilter->GetSize() == EnvMapCapture::kDefaultCubeFaceSize,
                    "prefilter cubemap base face size"))
            {
                return false;
            }

            if (!AssertTrue(environment != irradiance, "irradiance distinct from environment"))
            {
                return false;
            }

            if (!AssertTrue(prefilter != environment, "prefilter distinct from environment"))
            {
                return false;
            }

            if (!AssertTrue(prefilter != irradiance, "prefilter distinct from irradiance"))
            {
                return false;
            }

            ME_CORE_INFO("MaterialIR IBL GPU convolution + prefilter: PASSED.");
            return true;
        }

        bool VerifyNormalMapWorkflow(const MaterialCompileContext& ctx)
        {
            Material material;
            material.m_Graph = NewObject<MaterialEdGraph>("", &material);
            MaterialEdGraph& graph = *material.m_Graph;

            MaterialEdGraphNode& texCoord = graph.AddNode<MaterialGraphNodeDef_TextureCoordinate>();
            MaterialEdGraphNode& albedoTex = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            static_cast<MaterialGraphNodeDef_TextureObject*>(albedoTex.GetNodeDef())->TextureSlotIndex = 0;
            MaterialEdGraphNode& albedoSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();
            MaterialEdGraphNode& normalTex = graph.AddNode<MaterialGraphNodeDef_TextureObject>();
            MaterialGraphNodeDef_TextureObject* normalTexDef =
                static_cast<MaterialGraphNodeDef_TextureObject*>(normalTex.GetNodeDef());
            normalTexDef->ParameterName = "NormalMap";
            normalTexDef->TextureSlotIndex = 1;
            MaterialEdGraphNode& normalSample = graph.AddNode<MaterialGraphNodeDef_TextureSample>();
            MaterialEdGraphNode& normalUnpack = graph.AddNode<MaterialGraphNodeDef_NormalUnpack>();
            MaterialEdGraphNode& outputNode = graph.AddNode<MaterialGraphNodeDef_MaterialOutput>();

            const MaterialShadingModel shadingModel = MaterialShadingModel::BlinnPhong;
            const MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

            graph.ConnectPins(albedoTex, 0, albedoSample, 0, shadingModel, blendMode);
            graph.ConnectPins(texCoord, 0, albedoSample, 1, shadingModel, blendMode);
            graph.ConnectPins(normalTex, 0, normalSample, 0, shadingModel, blendMode);
            graph.ConnectPins(texCoord, 0, normalSample, 1, shadingModel, blendMode);
            graph.ConnectPins(normalSample, 1, normalUnpack, 0, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                albedoSample, 1, outputNode, MP_Albedo, shadingModel, blendMode);
            graph.ConnectToMaterialProperty(
                normalUnpack, 0, outputNode, MP_Normal, shadingModel, blendMode);

            const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
                graph,
                shadingModel,
                blendMode,
                ctx);
            if (!compiled.Succeeded)
            {
                for (const MaterialCompileDiagnostic& diagnostic : compiled.Diagnostics)
                {
                    ME_CORE_ERROR("MaterialIR NormalMap workflow diagnostic: {}", diagnostic.Message);
                }
                return false;
            }

            const std::string& body = compiled.Stages[Stage_Fragment].Body;
            bool passed = AssertAllContains(compiled.FullFragmentShader, {
                    { "BuildWorldNormalFromTangentSpace", "NormalMap uses TBN" },
                    { "u_Texture1", "NormalMap texture slot 1" },
                })
                && AssertAllContains(body, {
                    { "FragmentMaterialInputs.Normal =", "Normal unpack feeds MP_Normal" },
                    { "2.000000", "NormalUnpack scale * 2" },
                    { "1.000000", "NormalUnpack bias - 1" },
                })
                && AssertTrue(
                    body.find("texture(u_Texture1") != std::string::npos,
                    "NormalMap samples u_Texture1");

            if (!passed)
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR NormalMap workflow compile content check FAILED.");
                return false;
            }

            if (!VerifySmokeGpuCompile(compiled))
            {
                LogCompiledShaders(compiled);
                ME_CORE_ERROR("MaterialIR NormalMap workflow GPU compile FAILED.");
                return false;
            }

            ME_CORE_INFO("MaterialIR NormalMap workflow: compile + GPU link PASSED.");
            return true;
        }

        bool VerifyGoldenMaterialIRSmokeMemtl()
        {
            std::string diskError;
            if (!VerifyGoldenMaterialIRSmokeMemtlOnDisk(&diskError))
            {
                ME_CORE_ERROR("MaterialIR golden asset on-disk check failed: {}", diskError);
                return false;
            }

            ME_CORE_INFO("MaterialIR golden asset on-disk fields PASSED (m_ShadingModel=1, m_BlendMode=0).");
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

    const std::string& GetMaterialIRTestEngineDefaultAssetsRoot()
    {
        return g_MaterialIRTestEngineDefaultAssetsRoot;
    }

    bool RunMaterialIRSmokeTests(int argc, char** argv)
    {
        g_MaterialIRTestEngineDefaultAssetsRoot.clear();

        if (EnsureReflectionReadyForMaterialIRTest())
        {
            EngineConfig engineConfig;
            if (PathRegistry::Get().LoadEngineConfiguration(argc, argv, engineConfig))
            {
                g_MaterialIRTestEngineDefaultAssetsRoot =
                    PathRegistry::Get().GetEngineDefaultAssetsRootString();
                ME_CORE_INFO(
                    "MaterialIR test: EngineDefaultAssetsRoot = '{}'",
                    g_MaterialIRTestEngineDefaultAssetsRoot);
            }
            else
            {
                ME_CORE_WARN(
                    "MaterialIR test: EngineConfig load failed; continuing IR tests without EngineDefaultAssetsRoot.");
            }
        }
        else
        {
            ME_CORE_WARN(
                "MaterialIR test: reflection not ready; skipping EngineConfig load (IR smoke tests still run).");
        }

        MaterialIRTestObjectManagerScope objectManagerScope;

        if (!VerifyGoldenMaterialIRSmokeMemtl())
        {
            return false;
        }

        Material smokeMaterial;
        PopulateSmokeMaterialGraph(smokeMaterial);

        bool passed = AssertTrue(
            smokeMaterial.m_ShadingModel == MaterialShadingModel::BlinnPhong,
            "in-memory smoke m_ShadingModel BlinnPhong")
            && AssertTrue(
                smokeMaterial.m_BlendMode == MaterialBlendMode::Opaque,
                "in-memory smoke m_BlendMode Opaque");
        if (!passed)
        {
            return false;
        }

        const MaterialGraphNodeDef_MaterialOutput* outputNode =
            smokeMaterial.m_Graph ? FindMaterialOutputNode(*smokeMaterial.m_Graph) : nullptr;
        if (outputNode == nullptr)
        {
            ME_CORE_ERROR("MaterialIR test: smoke graph has no MaterialOutput node.");
            return false;
        }

        if (!VerifyPropertyBindingLayer(*smokeMaterial.m_Graph, *outputNode))
        {
            return false;
        }

        MaterialCompileContext ctx;
        if (!g_MaterialIRTestEngineDefaultAssetsRoot.empty())
        {
            ctx.EngineDefaultAssetsRootOverride = g_MaterialIRTestEngineDefaultAssetsRoot;
        }

        const MaterialCompileResult blinnPhongCompiled = MaterialCompiler::CompileForDiagnostics(
            *smokeMaterial.m_Graph,
            MaterialShadingModel::BlinnPhong,
            MaterialBlendMode::Opaque,
            ctx);
        if (!blinnPhongCompiled.Succeeded)
        {
            for (const MaterialCompileDiagnostic& diagnostic : blinnPhongCompiled.Diagnostics)
            {
                ME_CORE_ERROR("MaterialIR BlinnPhong diagnostic: {}", diagnostic.Message);
            }
            return false;
        }

        if (!AssertAllContains(blinnPhongCompiled.FullFragmentShader, {
                { "CalcDirLightGraph", "Phong directional light (graph terminology)" },
                { "MaterialPhongShininessFromRoughness", "Phong shininess from roughness" },
                { "MaterialSpecularFromMetallic", "Metallic maps to legacy specular" },
                { "BuildWorldNormalFromTangentSpace", "BlinnPhong TBN world normal" },
                { "FragmentMaterialInputs.Normal", "BlinnPhong uses material Normal input" },
                { "FragmentMaterialInputs.AO", "BlinnPhong uses material AO input" },
                { "dirLightResult + pointLightResult + spotLightResult", "Phong per-light accumulation" },
                { "u_DirLightShadowMap", "Phong fragment directional shadow sampler" },
            })
            || !AssertAllContains(blinnPhongCompiled.FullVertexShader, {
                { "layout(location = 3) in vec4 a_Tangent", "BlinnPhong vertex tangent attribute" },
                { "v_WorldTangent", "world tangent varying" },
                { "v_TangentSign", "tangent handedness varying" },
            })
            || !AssertAllContains(blinnPhongCompiled.Stages[Stage_Fragment].Body, {
                { "FragmentMaterialInputs.Normal = vec3(0.000000, 0.000000, 1.000000)", "default TSN +Z" },
                { "FragmentMaterialInputs.AO = 1.000000", "default AO" },
                { "FragmentMaterialInputs.Metallic = u_ScalarParam0", "BlinnPhong metallic scalar" },
            })
            || !VerifySmokeGpuCompile(blinnPhongCompiled))
        {
            LogCompiledShaders(blinnPhongCompiled);
            ME_CORE_ERROR("MaterialIR BlinnPhong smoke FAILED during compile or GPU link.");
            return false;
        }

        ME_CORE_INFO("MaterialIR BlinnPhong smoke: GPU vertex/fragment compile + link PASSED.");

        if (!VerifyConstant3ToNormalBlinnPhong(ctx))
        {
            return false;
        }

        if (!VerifyIfThenElseAlbedoBlinnPhong(ctx))
        {
            return false;
        }

        if (!VerifyTextureSampleSharedByTwoOutputs(ctx))
        {
            return false;
        }

        if (!VerifyDivideByZeroPoisonDiagnostic(ctx))
        {
            return false;
        }

        if (!VerifyNormalMapWorkflow(ctx))
        {
            return false;
        }

        if (!VerifyTextureCubeRHICreation())
        {
            return false;
        }

        if (!VerifyPBRWorkflow(ctx))
        {
            return false;
        }

        if (!VerifyEngineIBLEnvironmentInit())
        {
            return false;
        }

        if (!VerifyIBLEnvironmentFallbackChain())
        {
            return false;
        }

        if (!VerifyIBLGpuConvolutionAndPrefilter())
        {
            return false;
        }

        if (!VerifyFragmentStructMatchesCapability(blinnPhongCompiled, MaterialShadingModel::BlinnPhong, MaterialBlendMode::Opaque))
        {
            return false;
        }

        smokeMaterial.m_ShadingModel = MaterialShadingModel::Unlit;
        MaterialCapabilityUtil::PruneInvalidMaterialOutputLinks(smokeMaterial);

        if (!MaterialValueTypeUtil::ValidateGraphPinConnections(
                *smokeMaterial.m_Graph,
                smokeMaterial.m_ShadingModel,
                smokeMaterial.m_BlendMode,
                nullptr))
        {
            ME_CORE_ERROR("MaterialIR test: smoke graph pin type validation failed.");
            return false;
        }

        MaterialEdGraphNode* outputEdNode = smokeMaterial.m_Graph->FindEdNodeByNodeDef(outputNode);
        MaterialEdGraphNode* floatConstantEdNode = nullptr;
        for (const std::shared_ptr<MaterialEdGraphNode>& edNode : smokeMaterial.m_Graph->m_Nodes)
        {
            if (edNode && dynamic_cast<MaterialGraphNodeDef_Constant*>(edNode->GetNodeDef()) != nullptr)
            {
                floatConstantEdNode = edNode.get();
                break;
            }
        }

        if (outputEdNode == nullptr || floatConstantEdNode == nullptr)
        {
            ME_CORE_ERROR("MaterialIR test: missing nodes for pin compatibility check.");
            return false;
        }

        constexpr int32_t kAlbedoInputIndex = 0;
        if (smokeMaterial.m_Graph->CanConnectPins(
                *floatConstantEdNode,
                0,
                *outputEdNode,
                kAlbedoInputIndex,
                smokeMaterial.m_ShadingModel,
                smokeMaterial.m_BlendMode,
                nullptr))
        {
            ME_CORE_ERROR("MaterialIR test: float output must not connect to Albedo (float3) input.");
            return false;
        }

        ME_CORE_INFO("MaterialIR pin type connection checks PASSED.");

        const MaterialCompileResult compiled = MaterialCompiler::CompileForDiagnostics(
            *smokeMaterial.m_Graph,
            smokeMaterial.m_ShadingModel,
            smokeMaterial.m_BlendMode,
            ctx);
        if (!VerifySmokeCompileResult(compiled))
        {
            return false;
        }

        if (!VerifyFragmentStructMatchesCapability(compiled, MaterialShadingModel::Unlit, MaterialBlendMode::Opaque))
        {
            return false;
        }

        if (!VerifyTranslucentUnlitCompile(ctx))
        {
            return false;
        }

        ME_CORE_INFO("MaterialIR smoke tests PASSED (graph binding + compile diagnostics + golden asset).");
        return true;
    }
}
