#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Render/OpenGL/OpenGLRHI.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHITexture.h"
#include "Render/EngineRHITextureUtils.h"
#include "Render/RenderPipeline/RenderPasses/GraphClearPass.h"
#include "Runtime/Function/Render/Texture2DRenderTarget.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Access/ObjectManagerTestAccess.h"
#include "EngineTestFixture.h"
#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace minEngine
{
    namespace
    {
        class ScopedHeadlessGlContext
        {
        public:
            ScopedHeadlessGlContext()
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
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                m_Window = glfwCreateWindow(1, 1, "RenderGraphBake", nullptr, nullptr);
                if (m_Window == nullptr)
                {
                    return;
                }

                glfwMakeContextCurrent(m_Window);
                if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
                {
                    glfwDestroyWindow(m_Window);
                    m_Window = nullptr;
                }
            }

            ~ScopedHeadlessGlContext()
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

            bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

        private:
            GLFWwindow* m_Window = nullptr;
            bool m_OwnsGlfw = false;
        };

        class BakeProbePass : public IRenderPass
        {
        public:
            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::Absolute;
                color.SizeX = 64.0f;
                color.SizeY = 64.0f;
                color.Format = TextureFormat::RGBA8;
                self.AddColorOutput(kRDGBackbuffer, color);
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
            }
        };

        class ProducerPass : public IRenderPass
        {
        public:
            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::SwapchainRelative;
                color.SizeX = 1.0f;
                color.SizeY = 1.0f;
                color.Format = TextureFormat::RGBA8;
                self.AddColorOutput(kRDGSceneColor, color);
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
            }
        };

        class ConsumerPass : public IRenderPass
        {
        public:
            explicit ConsumerPass(std::string* trace)
                : m_Trace(trace)
            {
            }

            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                self.AddTextureInput(kRDGSceneColor);
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::SwapchainRelative;
                color.SizeX = 1.0f;
                color.SizeY = 1.0f;
                color.Format = TextureFormat::RGBA8;
                self.AddColorOutput(kRDGBackbuffer, color, kRDGSceneColor);
            }

            void Prepare(RenderGraph& graph) override
            {
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "PC;";
                }
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "BC;";
                }
            }

        private:
            std::string* m_Trace = nullptr;
        };

        class TracingProducerPass : public IRenderPass
        {
        public:
            explicit TracingProducerPass(std::string* trace)
                : m_Trace(trace)
            {
            }

            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::SwapchainRelative;
                color.SizeX = 1.0f;
                color.SizeY = 1.0f;
                color.Format = TextureFormat::RGBA8;
                self.AddColorOutput(kRDGSceneColor, color);
            }

            void Prepare(RenderGraph& graph) override
            {
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "PP;";
                }
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "BP;";
                }
            }

        private:
            std::string* m_Trace = nullptr;
        };

        class ShadowAtlasWriterPass : public IRenderPass
        {
        public:
            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                RDGAttachmentInfo depth{};
                depth.SizeClass = RDGSizeClass::Absolute;
                depth.SizeX = 64.0f;
                depth.SizeY = 64.0f;
                depth.Format = TextureFormat::DEPTH32;
                depth.Dimension = RHITextureDimension::Texture2DArray;
                depth.Layers = 4;
                self.SetDepthStencilOutput(kRDGDirShadowAtlas, depth);
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
            }
        };

        class LitScenePass : public IRenderPass
        {
        public:
            explicit LitScenePass(std::string* trace)
                : m_Trace(trace)
            {
            }

            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                self.AddTextureInput(kRDGDirShadowAtlas);
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::SwapchainRelative;
                color.SizeX = 1.0f;
                color.SizeY = 1.0f;
                color.Format = TextureFormat::RGBA8;
                RDGAttachmentInfo depth{};
                depth.SizeClass = RDGSizeClass::SwapchainRelative;
                depth.SizeX = 1.0f;
                depth.SizeY = 1.0f;
                depth.Format = TextureFormat::DEPTH24STENCIL8;
                self.AddColorOutput(kRDGSceneColor, color);
                self.SetDepthStencilOutput(kRDGSceneDepth, depth);
            }

            void Prepare(RenderGraph& graph) override
            {
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "LP;";
                }
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
                if (m_Trace != nullptr)
                {
                    *m_Trace += "BL;";
                }
            }

        private:
            std::string* m_Trace = nullptr;
        };

        class SceneClearOutputPass : public IRenderPass
        {
        public:
            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::SwapchainRelative;
                color.SizeX = 1.0f;
                color.SizeY = 1.0f;
                color.Format = TextureFormat::RGBA8;
                RDGAttachmentInfo depth{};
                depth.SizeClass = RDGSizeClass::SwapchainRelative;
                depth.SizeX = 1.0f;
                depth.SizeY = 1.0f;
                depth.Format = TextureFormat::DEPTH24STENCIL8;
                self.AddColorOutput(kRDGSceneColor, color);
                self.SetDepthStencilOutput(kRDGSceneDepth, depth);
            }

            // Mirrors BasePass: clears scene color using the frame context DrawDesc ClearColor/DepthClear.
            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                const RenderGraphFrameContext& ctx = graph.GetFrameContext();
                RHITexture* color = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneColor));
                RHITexture* depth = graph.TryGetPhysicalTexture(&graph.GetTextureResource(kRDGSceneDepth));
                if (color == nullptr || depth == nullptr)
                {
                    return;
                }
                const SceneDrawDesc* desc = ctx.DrawDesc;
                RHIRenderPassInfo passInfo = MakeSceneRenderPassInfo(
                    color,
                    depth,
                    true,
                    (desc != nullptr) ? desc->ClearColor : nullptr,
                    (desc != nullptr) ? desc->DepthClear : 1.0f);
                cmdList.BeginRenderPass(passInfo);
                cmdList.SetViewport(0, 0, color->GetDesc().Width, color->GetDesc().Height);
                cmdList.EndRenderPass();
            }
        };

        class OrphanTextureConsumerPass : public IRenderPass
        {
        public:
            void SetupDependencies(RenderPass& self, RenderGraph& graph) override
            {
                (void)graph;
                self.AddTextureInput("OrphanAtlas");
                RDGAttachmentInfo color{};
                color.SizeClass = RDGSizeClass::Absolute;
                color.SizeX = 16.0f;
                color.SizeY = 16.0f;
                color.Format = TextureFormat::RGBA8;
                self.AddColorOutput(kRDGBackbuffer, color);
            }

            void BuildRenderPass(RHICommandList& cmdList, RenderGraph& graph) override
            {
                (void)cmdList;
                (void)graph;
            }
        };
    }

    TEST_CASE("render-graph: bake then SetupAttachments yields physical texture")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        BakeProbePass probe;
        RenderPass& pass = graph.AddPass("Probe");
        pass.SetImplementation(&probe);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(64, 64);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);

        RDGTextureResource& backbuffer = graph.GetTextureResource(kRDGBackbuffer);
        CHECK(backbuffer.GetPhysicalIndex() != RDGResource::kUnused);
        RHITexture* physical = graph.GetPhysicalTexture(backbuffer);
        REQUIRE(physical != nullptr);
        CHECK(physical->GetDesc().Width == 64);
        CHECK(physical->GetDesc().Height == 64);
        CHECK(physical->GetDesc().Format == TextureFormat::RGBA8);
    }

    TEST_CASE("render-graph: bake orders producer before consumer")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        std::string trace;

        TracingProducerPass producerImpl(&trace);
        ConsumerPass consumerImpl(&trace);

        RenderPass& producer = graph.AddPass("Producer");
        producer.SetImplementation(&producerImpl);
        RenderPass& consumer = graph.AddPass("Consumer");
        consumer.SetImplementation(&consumerImpl);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(32, 32);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);

        RHICommandList cmdList(&rhi);
        graph.EnqueueRenderPasses(cmdList);

        CHECK(trace == "PP;PC;BP;BC;");
        CHECK(graph.GetPhysicalTexture(graph.GetTextureResource(kRDGSceneColor)) != nullptr);
        CHECK(graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer)) != nullptr);
        CHECK(
            graph.GetTextureResource(kRDGSceneColor).GetPhysicalIndex()
            == graph.GetTextureResource(kRDGBackbuffer).GetPhysicalIndex());
    }

    TEST_CASE("render-graph: bake rejects missing backbuffer writer")
    {
        RenderGraph graph;
        ProducerPass producerImpl;
        RenderPass& producer = graph.AddPass("Producer");
        producer.SetImplementation(&producerImpl);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(16, 16);

        CHECK_THROWS_AS(graph.Bake(), std::logic_error);
    }

    TEST_CASE("render-graph: bake rejects texture input without writer")
    {
        RenderGraph graph;
        OrphanTextureConsumerPass consumerImpl;
        RenderPass& consumer = graph.AddPass("OrphanConsumer");
        consumer.SetImplementation(&consumerImpl);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(16, 16);

        CHECK_THROWS_AS(graph.Bake(), std::logic_error);
    }

    TEST_CASE("render-graph: bake orders shadow atlas writer before lit scene pass")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        std::string trace;

        ShadowAtlasWriterPass shadowImpl;
        LitScenePass litImpl(&trace);

        RenderPass& shadow = graph.AddPass("Shadow.Dir");
        shadow.SetImplementation(&shadowImpl);
        RenderPass& lit = graph.AddPass("Scene.Opaque");
        lit.SetImplementation(&litImpl);

        graph.SetBackbufferSource(kRDGSceneColor);
        graph.SetBackbufferDimensions(32, 32);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);

        const std::vector<uint32_t>& stack = graph.GetPassStack();
        REQUIRE(stack.size() == 2);
        CHECK(stack[0] == shadow.GetIndex());
        CHECK(stack[1] == lit.GetIndex());

        const auto& deps = graph.GetPassDependencies();
        REQUIRE(lit.GetIndex() < deps.size());
        CHECK(deps[lit.GetIndex()].count(shadow.GetIndex()) == 1);

        RHICommandList cmdList(&rhi);
        graph.EnqueueRenderPasses(cmdList);
        CHECK(trace == "LP;BL;");
    }

    TEST_CASE("render-graph: vertical slice clear writes observable color")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        GraphClearPass clearPass;
        RenderPass& pass = graph.AddPass("VerticalSlice.Clear");
        pass.SetImplementation(&clearPass);

        graph.SetBackbufferSource(kRDGSceneColor);
        graph.SetBackbufferDimensions(8, 8);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);

        RHICommandList cmdList(&rhi);
        graph.EnqueueRenderPasses(cmdList);

        RHITexture* color = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGSceneColor));
        REQUIRE(color != nullptr);

        float expected[4]{};
        REQUIRE(clearPass.GetClearColor(0, expected));

        const GLuint textureId = color->GetNativeHandle();
        REQUIRE(textureId != 0);

        std::vector<uint8_t> pixels(static_cast<size_t>(8) * 8 * 4);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        const auto nearByte = [](uint8_t actual, float expected01) {
            const int expectedByte = static_cast<int>(std::lround(expected01 * 255.0f));
            return std::abs(static_cast<int>(actual) - expectedByte) <= 2;
        };

        CHECK(nearByte(pixels[0], expected[0]));
        CHECK(nearByte(pixels[1], expected[1]));
        CHECK(nearByte(pixels[2], expected[2]));
        CHECK(nearByte(pixels[3], expected[3]));
    }

    TEST_CASE("render-graph: distinct ViewKeys keep distinct physical textures at same size")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        BakeProbePass probe;
        RenderPass& pass = graph.AddPass("Probe");
        pass.SetImplementation(&probe);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(64, 64);

        graph.SetActiveViewKey(10, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);
        RHITexture* textureA = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer));
        REQUIRE(textureA != nullptr);

        graph.SetActiveViewKey(11, nullptr);
        graph.Bake();
        graph.SetupAttachments(rhi);
        RHITexture* textureB = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer));
        REQUIRE(textureB != nullptr);

        CHECK(textureA != textureB);

        graph.SetActiveViewKey(10, nullptr);
        RHITexture* textureAAgain = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer));
        CHECK(textureAAgain == textureA);
    }

    TEST_CASE("render-graph: DrawDesc transparent clear color reaches the backbuffer")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        SceneClearOutputPass clearPass;
        RenderPass& pass = graph.AddPass("Scene.Clear");
        pass.SetImplementation(&clearPass);

        graph.SetBackbufferSource(kRDGSceneColor);
        graph.SetBackbufferDimensions(8, 8);
        graph.SetActiveViewKey(1, nullptr);

        // Transparent clear: RGBA = (0,0,0,0), the RTT icon background.
        SceneDrawDesc desc{};
        desc.ClearColor[0] = 0.0f;
        desc.ClearColor[1] = 0.0f;
        desc.ClearColor[2] = 0.0f;
        desc.ClearColor[3] = 0.0f;
        desc.DepthClear = 1.0f;
        RenderGraphFrameContext ctx;
        ctx.DrawDesc = &desc;
        graph.SetFrameContext(ctx);

        graph.Bake();
        graph.SetupAttachments(rhi);
        RHICommandList cmdList(&rhi);
        graph.EnqueueRenderPasses(cmdList);

        RHITexture* color = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGSceneColor));
        REQUIRE(color != nullptr);

        const GLuint textureId = color->GetNativeHandle();
        REQUIRE(textureId != 0);

        std::vector<uint8_t> pixels(static_cast<size_t>(8) * 8 * 4);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        // Every pixel must be fully transparent black after the clear.
        bool allTransparent = true;
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            if (pixels[i] != 0u || pixels[i + 1] != 0u || pixels[i + 2] != 0u || pixels[i + 3] != 0u)
            {
                allTransparent = false;
                break;
            }
        }
        CHECK(allTransparent);
    }

    TEST_CASE("render-graph: RegisterExternalTexture installs caller SceneColor")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        BakeProbePass probe;
        RenderPass& pass = graph.AddPass("Probe");
        pass.SetImplementation(&probe);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(64, 64);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();

        RHITextureRef external = rhi.RHICreateTexture2D(
            MakeTexture2DRenderTargetDesc(64, 64, TextureFormat::RGBA8), nullptr);
        REQUIRE(external != nullptr);

        std::string error;
        REQUIRE(graph.RegisterExternalTexture(kRDGBackbuffer, external, &error));
        CHECK(error.empty());
        graph.SetupAttachments(rhi);

        RHITexture* physical = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer));
        REQUIRE(physical != nullptr);
        CHECK(physical == external.get());

        graph.UnregisterExternalTexture(kRDGBackbuffer);
        graph.SetupAttachments(rhi);
        RHITexture* recreated = graph.GetPhysicalTexture(graph.GetTextureResource(kRDGBackbuffer));
        REQUIRE(recreated != nullptr);
        CHECK(recreated != external.get());
    }

    TEST_CASE("render-graph: RegisterExternalTexture rejects size mismatch")
    {
        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        RenderGraph graph;
        BakeProbePass probe;
        RenderPass& pass = graph.AddPass("Probe");
        pass.SetImplementation(&probe);

        graph.SetBackbufferSource(kRDGBackbuffer);
        graph.SetBackbufferDimensions(64, 64);
        graph.SetActiveViewKey(1, nullptr);
        graph.Bake();

        RHITextureRef wrongSize = rhi.RHICreateTexture2D(
            MakeTexture2DRenderTargetDesc(32, 32, TextureFormat::RGBA8), nullptr);
        REQUIRE(wrongSize != nullptr);

        std::string error;
        CHECK_FALSE(graph.RegisterExternalTexture(kRDGBackbuffer, wrongSize, &error));
        CHECK_FALSE(error.empty());
    }

    TEST_CASE("render-graph: Texture2DRenderTarget Create is a Texture2D with RT GPU")
    {
        EngineReflectionFixture reflection;
        REQUIRE(reflection.IsReflectionReady());

        ObjectManager objectManager;
        Testing::TestAccess<ObjectManager>::SetInstance(&objectManager);
        ObjectManager::Initialize();

        ScopedHeadlessGlContext glContext;
        REQUIRE(glContext.IsReady());

        OpenGLRHI rhi;
        std::shared_ptr<Texture2DRenderTarget> target =
            Texture2DRenderTarget::Create(rhi, 16, 16, TextureFormat::RGBA8);
        REQUIRE(target != nullptr);
        CHECK(target->GetWidth() == 16);
        CHECK(target->GetHeight() == 16);
        REQUIRE(target->GetRHITextureShared() != nullptr);

        std::shared_ptr<Texture2D> asTexture2D = target;
        REQUIRE(asTexture2D != nullptr);
        CHECK(asTexture2D->GetRHITexture() == target->GetRHITexture());

        const RHITextureCreateDesc& desc = target->GetRHITextureShared()->GetDesc();
        CHECK((desc.Flags & RHITextureCreateFlags::RenderTarget) != RHITextureCreateFlags::None);
        CHECK((desc.Flags & RHITextureCreateFlags::ShaderResource) != RHITextureCreateFlags::None);

        target.reset();
        asTexture2D.reset();
        ObjectManager::Shutdown();
        Testing::TestAccess<ObjectManager>::SetInstance(nullptr);
    }
}
