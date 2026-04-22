#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    class AssetMeta;

    ME_CLASS()
    class Asset : public MEObject
    {
    public:
        virtual ~Asset() = default;

        const AssetMeta* GetMeta() const { return m_Meta; }
        void SetMeta(AssetMeta* inMeta) { m_Meta = inMeta; }
    protected:
        AssetMeta* m_Meta = nullptr;
    };
}

#include "Asset.gen.h"