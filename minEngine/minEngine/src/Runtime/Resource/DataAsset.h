#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"

#include <string>

namespace minEngine
{
    class DataAssetLoader;

    /**
     * Thin editable definition asset (ASSET-F07).
     * Game modules derive and register their own typeId + handlers; Core stays mechanism-only.
     */
    ME_CLASS()
    class DataAsset : public Asset
    {
        ME_GENERATED_BODY()

    public:
        DataAsset() = default;
        ~DataAsset() override = default;

        const std::string& GetNotes() const { return m_Notes; }
        void SetNotes(std::string notes) { m_Notes = std::move(notes); }

    protected:
        friend class DataAssetLoader;

        /** Optional authoring note for Inspector smoke / docs; not gameplay semantics. */
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetNotes", Getter = "GetNotes"))
        std::string m_Notes;
    };
}

#include "DataAsset.gen.h"
