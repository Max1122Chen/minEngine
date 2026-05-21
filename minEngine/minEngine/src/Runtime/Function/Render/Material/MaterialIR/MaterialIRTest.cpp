#include "MaterialIRTest.h"

#include "Log/LogSystem.h"
#include "Runtime/EngineConfig.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Serialization/JsonArchive.h"
#include "Serialization/Serializer.h"
#include "../MaterialCompiler/MaterialCompiler.h"
#include "../MaterialEdGraph.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialPropertyUtil.h"
#include "../../Material.h"
#include "../MaterialTestGraph.h"
#include "Render/Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <string_view>

namespace minEngine
{
    namespace
    {
        // Duplicated from Editor::LoadEngineConfig for headless --material-ir-test (no Editor instance).
        // TODO: share with Editor once engine bootstrap owns config loading.
        static constexpr const char* kEngineConfigExtension = ".meconfig";

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

        // Same cwd + dev fallback path as Editor::LoadEngineConfig.
        bool TryLoadEngineConfigForMaterialIRTest(EngineConfig& outConfig)
        {
            std::filesystem::path cwd = std::filesystem::current_path();
            std::filesystem::path configPath = cwd / ("EngineConfig" + std::string(kEngineConfigExtension));
            if (!std::filesystem::exists(configPath))
            {
                ME_CORE_ERROR("Engine config file not found at current working directory: '{}'.", cwd.string());
                configPath = std::filesystem::path("D:/Dev/GitRepo/minEngine/minEngine/EngineConfig.meconfig");
                ME_CORE_WARN(
                    "MaterialIR test: using hardcoded engine config path '{}' instead of '{}'. "
                    "Temporary; align with Editor::LoadEngineConfig.",
                    configPath.string(),
                    cwd.string());
            }

            Serialization::JsonReaderArchive reader;
            Serialization::SerializeResult result = Serialization::Serializer::FromFile(
                configPath.string(),
                minEngine::Reflection::GetClassName<EngineConfig>(),
                &outConfig,
                reader,
                Serialization::SerializerOptions{
                    .enumAsString = true,
                    .strictTypeCheck = true,
                    .skipUnknownField = true,
                    .writeObjectTypeName = false,
                    .allowObjectPtrSerialization = true,
                });
            if (!result.ok)
            {
                ME_CORE_ERROR(
                    "MaterialIR test: failed to load engine config from '{}'. Error: {}. Field path: {}",
                    configPath.string(),
                    result.message,
                    result.fieldPath);
                return false;
            }
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

            passed = AssertTrue(result.ParameterLayout.Parameters.size() == 2, "parameter layout entry count")
                && passed;

            bool foundTextureLayout = false;
            bool foundScalarLayout = false;
            for (const MaterialShaderParameterDesc& parameterDesc : result.ParameterLayout.Parameters)
            {
                if (parameterDesc.Type == MaterialShaderParameterType::Texture2D
                    && parameterDesc.SlotIndex == 0
                    && parameterDesc.ShaderSymbolName == "u_Texture0")
                {
                    foundTextureLayout = true;
                }

                if (parameterDesc.Type == MaterialShaderParameterType::Scalar
                    && parameterDesc.SlotIndex == 0
                    && parameterDesc.ShaderSymbolName == "u_ScalarParam0")
                {
                    foundScalarLayout = true;
                }
            }

            passed = AssertTrue(foundTextureLayout, "layout u_Texture0") && passed;
            passed = AssertTrue(foundScalarLayout, "layout u_ScalarParam0") && passed;

            passed = AssertAllContains(result.IRDump, {
                { "; minEngine Material IR dump", "IR header" },
                { "SetMaterialOutput \"Albedo\"", "IR Albedo output" },
                { "ExternalInput", "IR texcoord input" },
                { "TextureRead", "IR texture sample" },
                { "UniformParameter", "IR scalar uniform" },
                { "Multiply", "IR multiply node" },
            }) && passed;

            passed = AssertAllContains(result.Stages[Stage_Fragment].Body, {
                { "FragmentMaterialInputs.Albedo =", "fragment body Albedo assign" },
                { "FragmentMaterialInputs.Metallic = u_ScalarParam0", "fragment body Metallic assign" },
                { "texture(u_Texture0, MaterialParameters.TexCoords[0])", "UE-style texcoord access" },
            }) && passed;

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

    bool RunMaterialIRSmokeTests()
    {
        g_MaterialIRTestEngineDefaultAssetsRoot.clear();

        if (EnsureReflectionReadyForMaterialIRTest())
        {
            EngineConfig engineConfig;
            if (TryLoadEngineConfigForMaterialIRTest(engineConfig))
            {
                g_MaterialIRTestEngineDefaultAssetsRoot = engineConfig.EngineDefaultAssetsRoot;
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

        Material smokeMaterial;
        PopulateSmokeMaterialGraph(smokeMaterial);
        smokeMaterial.m_ShadingModel = MaterialShadingModel::Unlit;

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

        const MaterialCompileResult compiled =
            MaterialCompiler::CompileForDiagnostics(*smokeMaterial.m_Graph, smokeMaterial.m_ShadingModel, ctx);
        if (!VerifySmokeCompileResult(compiled))
        {
            return false;
        }

        const MaterialCompileResult blinnPhongCompiled =
            MaterialCompiler::CompileForDiagnostics(*smokeMaterial.m_Graph, MaterialShadingModel::BlinnPhong, ctx);
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
                { "kMaterialPhongShininess", "legacy fixed shininess 32" },
                { "MaterialSpecularFromMetallic", "Metallic maps to legacy specular" },
                { "dirLightResult + pointLightResult + spotLightResult", "Phong per-light accumulation" },
                { "u_DirLightShadowMap", "Phong fragment directional shadow sampler" },
            })
            || !VerifySmokeGpuCompile(blinnPhongCompiled))
        {
            LogCompiledShaders(blinnPhongCompiled);
            ME_CORE_ERROR("MaterialIR BlinnPhong smoke FAILED during compile or GPU link.");
            return false;
        }

        ME_CORE_INFO("MaterialIR BlinnPhong smoke: GPU vertex/fragment compile + link PASSED.");

        ME_CORE_INFO("MaterialIR smoke tests PASSED (graph binding + compile diagnostics).");
        ME_CORE_INFO("Material asset tests were removed; redesign asset/scene integration tests separately.");
        return true;
    }
}
