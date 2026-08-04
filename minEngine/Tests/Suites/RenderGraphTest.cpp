#include "Render/RenderGraph/IRenderPass.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RenderPass.h"
#include "Render/OpenGL/OpenGLRHI.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RenderPipeline/RenderPasses/GraphClearPass.h"
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
        graph.Bake();
        graph.SetupAttachments(rhi, nullptr);

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
        graph.Bake();
        graph.SetupAttachments(rhi, nullptr);

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
        graph.Bake();
        graph.SetupAttachments(rhi, nullptr);

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
}
