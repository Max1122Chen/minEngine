#pragma once

#include "Core.h"

namespace minEngine
{
    /**
     * Frozen engine binding table (F03-P0定稿，F03-M1 实现真源).
     *
     * Rules:
     * - (set, binding) is the logical key; ShaderBinding is the OpenGL UBO binding point or texture unit.
     * - MaterialCompiler emits layout(set = kSetMaterial, binding = …) (Vulkan dialect).
     * - OpenGL SPIR-V compile flattens set= away in ShaderCompiler (DescriptorSet must be 0).
     * - Hand-authored engine GLSL may still use flat layout(binding = ShaderBinding) for set-0 passes.
     * - MaterialCompiler / engine pass layouts MUST read from this header — no duplicated slot literals.
     */
    namespace EngineShaderBindings
    {
        // --- Main scene set indices (per draw in BasePass / TranslucencyPass) ---
        constexpr uint32_t kSetSceneObject = 0;
        constexpr uint32_t kSetShadowIBL = 1;
        constexpr uint32_t kSetMaterial = 2;

        // --- Set 0: scene / object ---
        constexpr uint32_t kSet0_PerFrame = 0;
        constexpr uint32_t kSet0_Lights = 1;
        constexpr uint32_t kSet0_PerObject = 2;

        constexpr uint32_t kGL_PerFrameUBO = 0;
        constexpr uint32_t kGL_LightsUBO = 1;
        constexpr uint32_t kGL_PerObjectUBO = 2; // mat4 u_Model or small PerDraw UBO

        // --- Set 1: shadow + IBL (PBR only uses IBL bindings) ---
        constexpr uint32_t kSet1_DirShadowSRV = 0;
        constexpr uint32_t kSet1_DirLightViewProjs = 1;
        constexpr uint32_t kSet1_CascadeFarPlanes = 2;
        constexpr uint32_t kSet1_SpotLightViewProjs = 3;
        constexpr uint32_t kSet1_SpotShadow0 = 4;
        constexpr uint32_t kSet1_SpotShadow1 = 5;
        constexpr uint32_t kSet1_PointShadow0 = 6;
        constexpr uint32_t kSet1_PointShadow1 = 7;
        constexpr uint32_t kSet1_IBLIrradiance = 8;
        constexpr uint32_t kSet1_IBLPrefilter = 9;
        constexpr uint32_t kSet1_IBLBrdfLut = 10;

        constexpr uint32_t kGL_DirShadowTextureUnit = 8;
        constexpr uint32_t kGL_DirLightViewProjsUBO = 9;
        constexpr uint32_t kGL_CascadeFarPlanesUBO = 10;
        constexpr uint32_t kGL_SpotLightViewProjsUBO = 11;
        /** Spot/point shadow sampler units (layout description only; SlotIndex selects maps). */
        constexpr uint32_t kGL_SpotShadowBaseUnit = kGL_DirShadowTextureUnit + 1u;
        constexpr uint32_t kGL_PointShadowBaseUnit = kGL_SpotLightViewProjsUBO;
        /** IBL sampler units (keep clear of material 0..N and shadow maps). */
        constexpr uint32_t kGL_IBLIrradianceUnit = 4;
        constexpr uint32_t kGL_IBLPrefilterUnit = 5;
        constexpr uint32_t kGL_IBLBrdfLutUnit = 6;

        static_assert(
            kGL_SpotShadowBaseUnit == 9u,
            "Set1 spot shadow base unit must follow dir shadow unit");
        static_assert(
            kGL_PointShadowBaseUnit == 11u,
            "Set1 point shadow base unit must follow spot light VPs UBO binding");

        // --- Set 2: material (per instance; k grows with MaterialShaderParameterLayout) ---
        constexpr uint32_t kSet2_MaterialTextureBase = 0;
        constexpr uint32_t kSet2_MaterialParamsUBO = 8; // optional scalar UBO; texture count TBD in M1

        // --- Shadow depth pass (pass-local set 0) ---
        constexpr uint32_t kSetShadowPass = 0;
        constexpr uint32_t kShadowPass_LightViewProj = 0;
        constexpr uint32_t kShadowPass_PerObject = 1;
        constexpr uint32_t kShadowPass_Params = 2;
        constexpr uint32_t kGL_ShadowPassLightViewProjUBO = 8;
        constexpr uint32_t kGL_ShadowPassParamsUBO = 11;

        // --- Present / post-process (pass-local set 0) ---
        constexpr uint32_t kSetEnginePost = 0;
        constexpr uint32_t kEnginePost_SceneColorSRV = 0;
        constexpr uint32_t kEnginePost_Params = 1;
        constexpr uint32_t kGL_EnginePostSceneColorUnit = 0;
        constexpr uint32_t kGL_EnginePostParamsUBO = 1;

        // --- Sky background (pass-local set 0) ---
        constexpr uint32_t kSetSkyPass = 0;
        constexpr uint32_t kSkyPass_EnvironmentSRV = 0;
        constexpr uint32_t kSkyPass_FrameData = 1;
        constexpr uint32_t kGL_SkyEnvironmentUnit = 0;
        constexpr uint32_t kGL_SkyFrameDataUBO = 1;

        // --- EnvMap offline capture (pass-local; not main scene sets) ---
        constexpr uint32_t kSetEnvCapture = 0;
        constexpr uint32_t kEnvCapture_SourceSRV = 0;
        constexpr uint32_t kEnvCapture_FrameData = 1;
        constexpr uint32_t kGL_EnvCaptureSourceUnit = 0;
        constexpr uint32_t kGL_EnvCaptureFrameDataUBO = 1;
    }
}
