#pragma once
#include "Core.h"
#include "Math/Geometry/AABB.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Render/DrawCommands/MeshCollectContext.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

#include <memory>

namespace minEngine
{
    class PrimitiveComponent;
    class MaterialRenderProxy;
    class MeshBatchCollector;

    /**
     * Scene representation of a drawable primitive.
     * Current subclasses (StaticMesh / SkeletalMesh / Sprite) all submit a mesh,
     * so vertex buffers and the material proxy live here. Non-mesh proxies
     * (Widget, lights, sky) do not inherit this type.
     */
    class MINENGINE_API PrimitiveSceneProxy
    {
    public:
        PrimitiveSceneProxy() = default;
        virtual ~PrimitiveSceneProxy() = default;

        /**
         * Identity token for the component this proxy was created from.
         *
         * Removal commands match a proxy against a component by comparing this pointer; the render
         * role never dereferences it. The component is destroyed on the game role as soon as the
         * world tears down, while a removal command only takes effect after the frame is recorded,
         * so a draw that read through it would touch freed memory.
         */
        PrimitiveComponent* m_PrimitiveComponent{ nullptr };

        Transform m_Transform;
        bool m_CastShadow = true;

        /**
         * Bounds the render role sorts, culls and shadows with.
         *
         * Captured on the game role while the component is known to be alive - at proxy creation
         * and again with every transform update packet - so visibility never has to ask the
         * component for its box after the game role may already have destroyed it.
         */
        Math::Geometry::AABB m_WorldBounds{};

        /**
         * Set by a removal command on the render role once the proxy must stop producing output.
         *
         * The draw set used to be gated on m_PrimitiveComponent being non-null, which made a
         * game-role pointer write the render role's notion of "half removed". This flag carries
         * that state instead and is written only where the removal commands run.
         */
        bool m_bPendingRemove = false;

        /** Game role: records the world-space box the render role will read. */
        void SetWorldBounds(const Math::Geometry::AABB& worldBounds) { m_WorldBounds = worldBounds; }

        /**
         * Whether this primitive produces draw and shadow output, i.e. the component is active and
         * visible. Written by the game thread when the proxy is created and synced; the render
         * thread only reads it, so no pass has to dereference the component to learn the state.
         */
        bool m_Enabled = true;

        /**
         * Strong references to every render resource this proxy draws with.
         *
         * The proxy outlives the game thread's interest in the asset: removal is a queued command
         * applied on the render thread, while the component drops its asset as soon as it is
         * destroyed or re-pointed at another mesh. Holding the references here keeps the buffers
         * and the render-side material object alive for as long as a draw packet can still be built
         * from this proxy, instead of relying on the two threads to agree on a destruction order.
         */
        RHIBufferRef m_VertexBufferRef;
        RHIVertexInputLayoutRef m_VertexInputLayoutRef;
        RHIBufferRef m_IndexBufferRef;
        /**
         * The render-side material object this proxy draws with.
         *
         * A reference to the render object, not to the material asset: the asset can be destroyed
         * on the game role while this proxy is still queued for the render role, and the draw must
         * keep answering from the published state. Releasing it here is a reference count drop on
         * whatever role removes this proxy, which is why the object it points at holds no game
         * object and unregisters from nothing in the object system.
         */
        MaterialRenderProxyRef m_MaterialSource;

        /**
         * The material this proxy drew with before its last re-bind.
         *
         * A frame that recorded a command against the previous binding can still submit it after
         * the binding changed, so the proxy keeps that one material alive until the proxy itself
         * goes away and hands both to the frame.
         */
        MaterialRenderProxyRef m_PreviousMaterialSource;

        /** Frame views of the references above: read by draw collection, never owned here. */
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        /**
         * The proxy to draw with, resolved when the material owner was bound.
         *
         * A material builds exactly one render proxy and keeps it for its whole lifetime - a
         * recompile or a parameter write rebuilds that proxy's GPU state in place, never the proxy
         * object - so this pointer only moves when the owner itself is re-bound, which happens on
         * the game role in SetMaterialSource. Resolving it there is what keeps draw collection off
         * the material: the references above own it, but the render role no longer asks it for its
         * proxy.
         */
        MaterialRenderProxy* m_MaterialProxy = nullptr;

        MaterialRenderProxy* GetMaterialProxy() const { return m_MaterialProxy; }

        /**
         * Draw ranges captured from the mesh metadata when the proxy was synced.
         * Carried through the batch / draw command / draw packet so submission never has to ask a
         * GPU object how much it holds.
         */
        uint32_t m_IndexCount = 0;
        uint32_t m_VertexCount = 0;

        /** Binds the mesh geometry and its draw ranges; the proxy takes ownership of the refs. */
        void SetMeshResources(
            const RHIBufferRef& vertexBuffer,
            const RHIVertexInputLayoutRef& vertexInputLayout,
            const RHIBufferRef& indexBuffer,
            uint32_t indexCount,
            uint32_t vertexCount);

        /** Drops the geometry binding, and with it the references that kept it alive. */
        void ClearMeshResources();

        /** Binds the material owner; GetMaterialProxy() follows it. */
        void SetMaterialSource(const std::shared_ptr<MaterialInterface>& material);

        /**
         * World-space AABB for visibility/sorting (+).
         *
         * Returns the box captured on the game role; it reads no game object, so a proxy that
         * outlives its component still answers with the last bounds the component published.
         */
        virtual Math::Geometry::AABB ResolveWorldBounds() const;

        /** Local→world the draw is submitted with; the world bounds are cached separately. */
        virtual Matrix4 GetBoundsLocalToWorld() const;

        /**
         * Emit pass-agnostic mesh descriptions.
         * Default: one batch from the fields above when drawable (ignores context).
         * Multi-section proxies may use context.ViewFrustum to skip sub-bounds.
         */
        virtual void GetMeshBatches(MeshBatchCollector& collector, const MeshCollectContext& context) const;
    };
}
