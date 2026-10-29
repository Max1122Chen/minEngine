#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/UI/UITypes.h"

#include <memory>

namespace minEngine
{
    class MaterialRenderProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;
    class WidgetComponent;

    class WidgetSceneProxy
    {
    public:
        WidgetSceneProxy() = default;
        ~WidgetSceneProxy() = default;

        /**
         * Identity token for the widget this proxy was filled from.
         *
         * Only compared by the removal commands. The ScreenUI queue used to dereference it to walk
         * the owning GameObject for its canvas, which could run against a component the game role
         * had already destroyed, so everything the queue needs is synced below instead.
         */
        WidgetComponent* m_WidgetComponent = nullptr;

        /**
         * Set by a removal command on the render role once the proxy must stop producing a draw.
         *
         * Written only there. A game-role clear of the token above would leave the already-queued
         * removal nothing to match, and the widget would stay registered - and drawn - until the
         * scene it belongs to is torn down.
         */
        bool m_bPendingRemove = false;

        /**
         * Canvas context captured while the component was alive on the game role.
         *
         * A widget draws under the nearest canvas ancestor: whether that canvas is active, how it
         * maps reference pixels to viewport pixels, and which sort order it carries. Walking the
         * GameObject to find that out on the render role is the read this replaces.
         */
        bool m_bHasActiveCanvas = false;
        EUICanvasScaleMode m_CanvasScaleMode = EUICanvasScaleMode::Letterbox;
        Vector2 m_CanvasReferenceResolution{ 1920.0f, 1080.0f };
        int32_t m_CanvasSortOrder = 0;

        Vector2 m_TopLeftPx{ 0.0f, 0.0f };
        Vector2 m_SizePx{ 100.0f, 100.0f };
        Vector4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector4 m_UVRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        uint32_t m_StableOrder = 0;

        Texture2D* m_Texture = nullptr;

        /**
         * Strong references to the geometry and the render-side material object this widget draws
         * with.
         *
         * A widget's pixel mesh lives on the graphic component that built it, and a text update
         * replaces it; the material instance lives on the widget component. Both can be dropped on
         * the game thread while the proxy is still queued for removal here, so the proxy keeps the
         * buffers - and the material's render object rather than the material - alive instead of
         * pointing at whatever the game thread leaves behind.
         *
         * The vertex input layout is not held: both producers hand out a layout cached for the
         * process lifetime (SpriteQuadMesh for the shared quad, TextMeshBuilder for glyphs).
         */
        RHIBufferRef m_VertexBufferRef;
        RHIBufferRef m_IndexBufferRef;

        /** The render-side material object this widget draws with; see PrimitiveSceneProxy. */
        MaterialRenderProxyRef m_MaterialSource;

        /** The material this proxy drew with before its last re-bind; see PrimitiveSceneProxy. */
        MaterialRenderProxyRef m_PreviousMaterialSource;

        /** Frame views of the references above; read by the ScreenUI queue, never owned here. */
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;

        /** The proxy to draw with; see PrimitiveSceneProxy::m_MaterialProxy. */
        MaterialRenderProxy* m_MaterialProxy = nullptr;

        /** The proxy to draw with; resolved on the game role when the material owner was bound. */
        MaterialRenderProxy* GetMaterialProxy() const { return m_MaterialProxy; }

        /** Draw ranges captured from the mesh metadata when the proxy was assembled. */
        uint32_t m_IndexCount = 0;
        uint32_t m_VertexCount = 0;

        /** Binds the geometry and its draw ranges; the proxy takes ownership of the buffer refs. */
        void SetMeshResources(
            const RHIBufferRef& vertexBuffer,
            RHIVertexInputLayout* vertexInputLayout,
            const RHIBufferRef& indexBuffer,
            uint32_t indexCount,
            uint32_t vertexCount)
        {
            m_VertexBufferRef = vertexBuffer;
            m_IndexBufferRef = indexBuffer;

            m_VertexBuffer = m_VertexBufferRef.get();
            m_IndexBuffer = m_IndexBufferRef.get();
            m_VertexInputLayout = vertexInputLayout;

            m_IndexCount = indexCount;
            m_VertexCount = vertexCount;
        }

        /** Drops the geometry binding, and with it the references that kept it alive. */
        void ClearMeshResources() { SetMeshResources(nullptr, nullptr, nullptr, 0, 0); }

        /** Binds the material's render object; GetMaterialProxy() follows it. */
        void SetMaterialSource(const std::shared_ptr<MaterialInterface>& material)
        {
            MaterialRenderProxyRef resolved = material ? material->GetRenderProxyRef() : nullptr;
            if (m_MaterialSource == resolved)
            {
                return;
            }

            if (m_MaterialSource)
            {
                m_PreviousMaterialSource = std::move(m_MaterialSource);
            }

            m_MaterialSource = std::move(resolved);
            m_MaterialProxy = m_MaterialSource.get();
        }

        /**
         * When true, VB is in Widget-local pixels (top-left origin, Y down).
         * BuildScreenUIQueue uses translation + uniform letterbox scale (not unit-quad scale).
         */
        bool m_bLocalPixelMesh = false;

        bool m_bVisible = false;
    };
}
