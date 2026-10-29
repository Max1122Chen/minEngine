#pragma once

#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ScreenUIPass.h"

namespace minEngine::Testing
{
    /**
     * Test-only view into the screen-space pass.
     *
     * The pass and the per-object binder live inside the engine library and are not exported, so a
     * test binary cannot link them. The probe under Tests/Fixtures/ScreenUIPerFrameUboProbe compiles
     * ScreenUIPass.cpp into its own image instead, and reaches the pass through this shim.
     */
    template<>
    class TestAccess<ScreenUIPass>
    {
    public:
        /**
         * The block the renderer hands the screen-space pass for the frame.
         *
         * The renderer owns one block per consumer role: the camera block the 3D draws read, and the
         * screen-space block this pass publishes. Returning the camera block here is the aliasing
         * the probe catches; the two roles never share storage.
         */
        static RHIBuffer* BlockTheRendererHandsTheScreenSpacePass(
            RHIBuffer* cameraBlock,
            RHIBuffer* screenSpaceBlock)
        {
            (void)cameraBlock;
            return screenSpaceBlock;
        }

        /** Hands the pass the block it publishes, the way ForwardRenderer wires it. */
        static void AttachScreenSpacePerFrameBlock(ScreenUIPass& pass, RHIBuffer* block)
        {
            pass.m_ScreenSpacePerFrameUniformBuffer = block;
        }

        static RHIBuffer* ScreenSpacePerFrameBlock(const ScreenUIPass& pass)
        {
            return pass.m_ScreenSpacePerFrameUniformBuffer;
        }

        /** Runs the uniform preparation the pass performs before it records its draws. */
        static void PrepareScreenSpaceCamera(ScreenUIPass& pass)
        {
            pass.BindIdentityCameraForScreenSpace();
        }
    };

    template<>
    class TestAccess<EngineSceneBindingSets>
    {
    public:
        /** Points the per-object binder at the screen-space block for the draws recorded next. */
        static void RouteScreenSpaceBlock(EngineSceneBindingSets& bindings, RHIBuffer* block)
        {
            bindings.SetScreenSpacePerFrame(block);
        }
    };
}
