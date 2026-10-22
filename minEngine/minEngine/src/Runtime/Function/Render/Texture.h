#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/RHI/RHISampler.h"

namespace minEngine
{
    class AssetManager;
    class RHI;

    /** Legacy mirror enums: only TextureCube still stores sampling modes this way;
        Texture2D declares its modes as RHISamplerFilter / RHISamplerAddress fields. */
    enum class TextureWrapping
    {
        None = 0,
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder
    };

    enum class TextureFiltering
    {
        None = 0,
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    ME_CLASS()
    class Texture2D : public Asset
    {
        ME_GENERATED_BODY()
    public:
        Texture2D() = default;

        virtual ~Texture2D() = default;

        RHITexture* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        uint32_t GetChannels() const { return m_Channels; }

        RHISamplerFilter GetSamplerFilter() const { return m_SamplerFilter; }
        RHISamplerAddress GetSamplerAddress() const { return m_SamplerAddress; }

        /** Filtering axis only; the address mode keeps its current value. */
        MINENGINE_API void SetSamplerFilter(RHISamplerFilter filter);
        /** Address axis only; the filtering mode keeps its current value. */
        MINENGINE_API void SetSamplerAddress(RHISamplerAddress address);

        /** Derived from the two author fields; the sampler cache is keyed by it. */
        const RHISamplerDesc& GetPreferredSamplerDesc() const { return m_PreferredSamplerDesc; }

        static std::shared_ptr<Texture2D> CreateSolidRGBA(RHI& rhi, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
        static std::shared_ptr<Texture2D> CreateFromRGBA8(RHI& rhi, uint32_t width, uint32_t height, const uint8_t* rgba);

    protected:
        friend class Texture2DLoader;

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSamplerFilter", Getter = "GetSamplerFilter"))
        RHISamplerFilter m_SamplerFilter { RHISamplerFilter::Linear };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSamplerAddress", Getter = "GetSamplerAddress"))
        RHISamplerAddress m_SamplerAddress { RHISamplerAddress::Repeat };

        RHITextureRef m_RHITexture;
        uint32_t m_Width {0};
        uint32_t m_Height {0};
        uint32_t m_Channels {0};
        RHISamplerDesc m_PreferredSamplerDesc{};

    private:
        /** Single sink for the derived cache: takes the desc as given and keeps the
            author fields in step with it. Derived classes cannot reach it either, so
            nothing outside this class can leave the fields and the desc disagreeing. */
        void SetPreferredSamplerDesc(const RHISamplerDesc& desc);
        /** Recomputes the cached desc from the author fields. */
        void RebuildPreferredSamplerDesc();
    };

    class TextureCubeLoader;

    class TextureCube
    {
    public:
        TextureCube() = default;

        virtual ~TextureCube() = default;

        RHITexture* GetRHITexture() const { return m_RHITexture.get(); }
        uint32_t GetSize() const { return m_Size; }
        TextureWrapping GetWrapping() const { return m_Wrapping; }
        TextureFiltering GetFiltering() const { return m_Filtering; }

    protected:
        friend class TextureCubeLoader;

        RHITextureRef m_RHITexture;
        uint32_t m_Size = 0;
        uint32_t m_Channels = 0;
        TextureWrapping m_Wrapping = TextureWrapping::ClampToEdge;
        TextureFiltering m_Filtering = TextureFiltering::Linear;
    };

}

#include "Texture.gen.h"
