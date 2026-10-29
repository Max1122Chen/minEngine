#pragma once

#include "Core.h"
#include "Runtime/Function/Render/MaterialInterface.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/RHI/RHIShaderBinding.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/RHI/RHITextureViewCache.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace minEngine
{
    class Material;
    class MaterialInstance;
    class RHICommandList;
    class RHIShaderResourceView;

    /** One scalar parameter as the render role sees it. A negative slot is a name with no layout slot. */
    struct MaterialRenderProxyScalar
    {
        std::string Name;
        int SlotIndex = -1;
        float Value = 0.0f;
    };

    /**
     * One texture parameter, holding the GPU texture rather than the asset that owns it.
     *
     * Both sampler candidates travel with it: the compile-time override the layout asked for, and
     * the desc the texture prefers. The render role picks between them with these two values, so
     * it never has to ask the texture or the compiled parameter which one applies.
     */
    struct MaterialRenderProxyTexture
    {
        std::string Name;
        int SlotIndex = -1;
        RHITextureRef Texture;
        bool bOverrideSampler = false;
        RHISamplerDesc SamplerIntent{};
        RHISamplerDesc TextureSampler{};
    };

    /**
     * Immutable snapshot of everything one material needs to draw.
     *
     * Publishing builds a fresh snapshot and swaps it in with one atomic store; the render role
     * acquires a snapshot and reads it. Nothing in here is a game object and nothing in here is
     * mutable, so a snapshot stays valid - and keeps the GPU handles it names alive - for as long
     * as any holder keeps the reference, independently of the material that produced it.
     */
    struct MaterialRenderProxyData
    {
        /** Bumped by every publish of the same proxy, so the render role can tell snapshots apart. */
        uint64_t Serial = 0;
        uint32_t CompileGeneration = 0;
        /**
         * The compile finished: a program handle and a parameter layout exist. Whether the program
         * has been created yet is a question for the handle, not for this snapshot, which is taken
         * on the game role and cannot see a later frame start.
         */
        bool bCompiledForDraw = false;
        bool bCompiledForSkinnedDraw = false;
        bool bTranslucent = false;
        bool bMasked = false;

        RHIShaderRef Shader;
        RHIShaderRef SkinnedShader;
        RHIShaderBindingSetLayoutRef BindingSetLayout;

        std::vector<MaterialRenderProxyScalar> Scalars;
        std::vector<MaterialRenderProxyTexture> Textures;

        const MaterialRenderProxyScalar* FindScalarByName(const std::string& name) const
        {
            for (const MaterialRenderProxyScalar& entry : Scalars)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const MaterialRenderProxyScalar* FindScalarBySlot(int slotIndex) const
        {
            if (slotIndex < 0)
            {
                return nullptr;
            }
            for (const MaterialRenderProxyScalar& entry : Scalars)
            {
                if (entry.SlotIndex == slotIndex)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const MaterialRenderProxyTexture* FindTextureByName(const std::string& name) const
        {
            for (const MaterialRenderProxyTexture& entry : Textures)
            {
                if (entry.Name == name)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const MaterialRenderProxyTexture* FindTextureBySlot(int slotIndex) const
        {
            if (slotIndex < 0)
            {
                return nullptr;
            }
            for (const MaterialRenderProxyTexture& entry : Textures)
            {
                if (entry.SlotIndex == slotIndex)
                {
                    return &entry;
                }
            }
            return nullptr;
        }
    };

    using MaterialRenderProxyDataRef = std::shared_ptr<const MaterialRenderProxyData>;

    /** Reads of game-role material state, as counted for the render path. */
    struct MaterialGameObjectAccess
    {
        uint32_t Total = 0;
        /** By-name / by-slot parameter getters, which answer from the owning material. */
        uint32_t ParameterReads = 0;
        /** GetBaseMaterial(), the route back to the owning material. */
        uint32_t OwnerReads = 0;
        /** Publishing, which is the one legitimate reader of the game-role tables. */
        uint32_t PublishReads = 0;
    };

    /**
     * Render-side material binding view (UE FMaterialRenderProxy analogue).
     *
     * Created by Material / MaterialInstance and held through MaterialRenderProxyRef: the material
     * keeps one reference, and every scene proxy that draws with it keeps another. SceneProxy and
     * MeshDrawCommand still hold raw pointers, which is why the owners hold the reference - a raw
     * pointer inside a recorded draw command stays valid until the last reference is gone.
     *
     * Nothing here is a game object, so the last reference may be dropped on any role. The
     * destructor leaves the publish sweep behind its registry lock, drops the GPU references this
     * object owns - each of which posts its native release to the RHI retire queue - and does
     * nothing else: no ObjectManager entry, no game-role table, no material dereference. The
     * material it publishes from is reached through a raw pointer that only the game role's
     * publish path follows.
     *
     * The split runs through this class:
     *  - game role: write the material's tables, mark the proxy dirty, publish a snapshot;
     *  - render role: acquire a snapshot and draw from it, plus the GPU state this object owns
     *    (binding set, scalar UBO, SRV cache).
     * Publishing happens in bulk at the end of a game tick, not per Set* call, so a sprite that
     * writes ten parameters still produces one snapshot.
     */
    class MaterialRenderProxy
    {
    public:
        MaterialRenderProxy();
        virtual ~MaterialRenderProxy();

        /** Game role only: the material this proxy belongs to and publishes from. */
        virtual Material* GetBaseMaterial() const = 0;

        /**
         * Render role: answered from the published snapshot, never from the material.
         *
         * Two facts decide it, and only the second one is live: the snapshot says the compile
         * finished (a program handle and a parameter layout exist), and the handle says the
         * program itself has been created. A program queued for the graphics role is compiled but
         * not there, so this answers false - the draw has to be skipped rather than submitted
         * with a name that is still zero.
         */
        virtual bool IsCompiledForDraw() const;
        virtual bool IsCompiledForSkinnedDraw() const;
        virtual bool IsTranslucent() const;
        virtual bool IsMasked() const;
        virtual RHIShader* GetGPUShader() const;
        virtual RHIShader* GetGPUShaderSkinned() const;

        /**
         * The compile finished, whatever the graphics role has done with the program so far.
         *
         * Callers that pass the program on instead of drawing with it - deriving a material, or
         * latching a scene proxy's visibility for a frame that records after the next frame start
         * - ask this, because a queued program is a finished compile that becomes drawable before
         * anything is recorded.
         */
        bool HasCompiledProgram() const
        {
            const MaterialRenderProxyDataRef data = AcquirePublishedData();
            return data != nullptr && data->bCompiledForDraw;
        }

        /** The program is queued for the graphics role: compiled, not created, not failed. */
        bool IsProgramPendingMaterialization() const
        {
            const MaterialRenderProxyDataRef data = AcquirePublishedData();
            return data != nullptr && data->Shader != nullptr && !data->Shader->IsReady()
                && !data->Shader->HasNativeCreateFailed();
        }

        /** The program was handed to the graphics role and its creation failed there. */
        bool HasProgramCreateFailed() const
        {
            const MaterialRenderProxyDataRef data = AcquirePublishedData();
            return data != nullptr && data->Shader != nullptr && data->Shader->HasNativeCreateFailed();
        }
        virtual RHIShaderBindingSetLayout* GetMaterialShaderBindingSetLayout() const;
        virtual RHIShaderBindingSet* GetMaterialShaderBindingSet() const;

        /**
         * The snapshot to draw from: one atomic load of an immutable block.
         *
         * The returned reference keeps the block - and the GPU handles it names - alive for as long
         * as the caller holds it. Raw pointers returned by the getters above stay valid under the
         * same rule they always had: while the material that published them is alive.
         */
        virtual MaterialRenderProxyDataRef AcquirePublishedData() const;

        /** Game role: parameter reads on the owning material (see the probe below). */
        virtual bool GetScalarParameterValue(const std::string& name, float& outValue) const = 0;
        virtual bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const = 0;
        virtual bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const = 0;
        virtual bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const = 0;

        /** Game role: the material changed; publish at the end of this tick. */
        void MarkGameDataDirty();

        /** Game role: build and swap in a snapshot now. */
        virtual void PublishGameData();

        /** Game role: publish if this proxy or anything it derives from changed. */
        virtual void PublishPendingGameData();

        /**
         * Game role: the material or instance that publishes this proxy is being destroyed.
         *
         * A render-side owner can keep its reference past that point, so the proxy stops reading
         * its owner and keeps the snapshot it already published. Publishing again would walk a
         * pointer whose owner is gone and replace a drawable snapshot with an empty one.
         */
        virtual void DetachFromOwner();

        /** Game role: how many times this proxy published; dependants compare it. */
        uint64_t GetPublishSerial() const { return m_PublishSerial; }

        /** Render role: rebuild the GPU state this snapshot needs, then upload the scalar values. */
        virtual void BindForDraw(RHICommandList& cmdList);

        /** How many times the GPU binding set was rebuilt; the generation check must not rebuild twice. */
        virtual uint32_t GetBindingSetRebuildCount() const { return m_BindingSetRebuildCount; }

        /**
         * Turn counting of game-role reads on or off. Enabling resets the counters, so a test can
         * measure exactly one window of work and then read the numbers.
         */
        virtual void SetGameObjectAccessProbe(bool bEnabled) const;
        virtual MaterialGameObjectAccess ProbeGameObjectAccess() const;

    protected:
        /** Game role: fill the snapshot from the owning material's tables. */
        virtual void BuildGameData(MaterialRenderProxyData& outData) const = 0;

        /** Game role: whether a texture parameter now resolves to something the snapshot missed. */
        virtual bool HasStaleTextureReferences() const { return false; }

        void DropRenderState();

        void SyncScalarUniformBuffer(const MaterialRenderProxyData& data, RHICommandList& cmdList);
        void RebuildShaderBindingSet(const MaterialRenderProxyData& data, RHICommandList& cmdList);

        void NoteParameterRead() const;
        void NoteOwnerRead() const;
        void NotePublishRead() const;

        bool IsOwnerDetached() const { return m_bOwnerDetached; }

        /** Game role only; the render role never reads it. */
        bool m_bGameDataDirty = true;
        /** Game role only: set once the owner is gone, after which nothing publishes again. */
        bool m_bOwnerDetached = false;
        uint64_t m_PublishSerial = 0;

        /** Published snapshot: written by an atomic store on the game role, read by an atomic load. */
        std::shared_ptr<const MaterialRenderProxyData> m_Published;

        /** Render role owns everything below; the game role never touches it. */
        RHIShaderBindingSetRef m_MaterialShaderBindingSet;
        RHITextureViewCache m_TextureViewCache;
        std::vector<RHIShaderResourceViewRef> m_TextureSRVs;
        /** Pins GetOrCreateSampler results for Binding.Sampler lifetime. */
        std::vector<RHISamplerRef> m_BoundSamplers;
        RHIBufferRef m_ScalarParamsUBO;
        uint32_t m_ScalarParamsUBOSize = 0;

        uint32_t m_SeenCompileGeneration = 0;
        uint64_t m_SeenSerial = 0;
        const RHIShaderBindingSetLayout* m_BoundLayout = nullptr;
        /** The uniform buffer the built set points at; a rebuilt buffer needs a rebuilt set. */
        const RHIBuffer* m_BoundUniformBuffer = nullptr;
        struct BoundTextureKey
        {
            int SlotIndex = -1;
            const RHITexture* Texture = nullptr;
            RHISamplerDesc Sampler{};
        };
        std::vector<BoundTextureKey> m_BoundTextureKeys;
        uint32_t m_BindingSetRebuildCount = 0;

    };

    /** Default proxy owned by Material; publishes Material's default parameter tables. */
    class DefaultMaterialRenderProxy : public MaterialRenderProxy
    {
    public:
        explicit DefaultMaterialRenderProxy(Material& material);
        ~DefaultMaterialRenderProxy() override = default;

        DefaultMaterialRenderProxy(const DefaultMaterialRenderProxy&) = delete;
        DefaultMaterialRenderProxy& operator=(const DefaultMaterialRenderProxy&) = delete;

        Material* GetBaseMaterial() const override;
        void DetachFromOwner() override;

        bool GetScalarParameterValue(const std::string& name, float& outValue) const override;
        bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const override;
        bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const override;
        bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const override;

    protected:
        void BuildGameData(MaterialRenderProxyData& outData) const override;
        bool HasStaleTextureReferences() const override;

    private:
        /** Game role only: the material whose tables are read and published. */
        Material* m_Material = nullptr;
    };

    /**
     * Instance proxy (UE FMaterialInstanceResource analogue).
     *
     * A published snapshot is flattened at publish time: the parent's slots and values are copied
     * and this instance's overrides are applied on top, so a draw never walks a parent chain
     * through game objects. The parent reference this proxy keeps is a reference to the parent
     * *proxy*, not to the material that owns it.
     */
    class InstanceMaterialRenderProxy : public MaterialRenderProxy
    {
    public:
        explicit InstanceMaterialRenderProxy(MaterialInstance& instance);
        ~InstanceMaterialRenderProxy() override = default;

        InstanceMaterialRenderProxy(const InstanceMaterialRenderProxy&) = delete;
        InstanceMaterialRenderProxy& operator=(const InstanceMaterialRenderProxy&) = delete;

        /** Game role: the parent proxy whose snapshots feed this one. */
        void SetParentProxy(MaterialRenderProxy* parentProxy);

        void PublishPendingGameData() override;
        void DetachFromOwner() override;

        Material* GetBaseMaterial() const override;

        bool GetScalarParameterValue(const std::string& name, float& outValue) const override;
        bool GetTextureParameterValue(
            const std::string& name, std::shared_ptr<Texture2D>& outTexture) const override;
        bool GetScalarParameterValueBySlot(int slotIndex, float& outValue) const override;
        bool GetTextureParameterValueBySlot(
            int slotIndex, std::shared_ptr<Texture2D>& outTexture) const override;

    protected:
        void BuildGameData(MaterialRenderProxyData& outData) const override;
        bool HasStaleTextureReferences() const override;

    private:
        /** Game role only: follows the parent link when nothing set the proxy explicitly. */
        MaterialRenderProxy* ResolveParentProxy() const;

        /** Game role only: the instance whose override tables are published. */
        MaterialInstance* m_Instance = nullptr;
        mutable MaterialRenderProxy* m_ParentProxy = nullptr;
        uint64_t m_SeenParentPublishSerial = 0;
    };

    /**
     * The set of live proxies, and the game tick's bulk publish.
     *
     * The registry holds weak references, because a proxy's last reference can be dropped on the
     * render role: a removal lands there, and the frame's keep-alive slots rotate there. A sweep
     * therefore locks each entry and skips whatever is already gone instead of reaching into a
     * proxy that is mid-destruction. Register, Unregister and the sweep all run under one mutex,
     * and no role owns the registry.
     */
    class MaterialRenderProxyPublisher
    {
    public:
        /** Any role: take the proxy into the publish sweep. */
        static void Register(const MaterialRenderProxyRef& proxy);

        /** Any role, from the proxy's own destructor: leave the publish sweep. */
        static void Unregister(const MaterialRenderProxy* proxy);

        /** Game role, end of tick: publish every proxy that changed since the last one. */
        static void PublishPendingGameData();

        /** Proxies still alive in the registry, for diagnostics and tests. */
        static size_t GetRegisteredCount();
    };

    /**
     * Creates a proxy and puts it in the publish sweep in one step.
     *
     * Construction cannot register itself any more: the registry refers to what it sweeps through
     * a reference count, and no reference exists until the object is complete.
     */
    template <typename TProxy, typename... TArgs>
    MaterialRenderProxyRef MakeMaterialRenderProxy(TArgs&&... args)
    {
        MaterialRenderProxyRef proxy = std::make_shared<TProxy>(std::forward<TArgs>(args)...);
        MaterialRenderProxyPublisher::Register(proxy);
        return proxy;
    }
}