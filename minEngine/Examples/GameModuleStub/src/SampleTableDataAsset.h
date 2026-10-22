#pragma once

#include "GameModuleStubAPI.h"
#include "Runtime/Resource/DataAsset.h"

#include <string>
#include <vector>

namespace GameModuleStub
{
    ME_STRUCT()
    struct SampleTableRow
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string Id;

        ME_PROPERTY()
        int32_t Value = 0;
    };

    /**
     * Sample derived DataAsset for polymorphic load tests.
     * Uses the shared DataAsset pipeline (.medata); no custom type registration.
     */
    ME_CLASS()
    class SampleTableDataAsset : public minEngine::DataAsset
    {
        ME_GENERATED_BODY()

    public:
        SampleTableDataAsset() = default;
        ~SampleTableDataAsset() override = default;

        const std::vector<SampleTableRow>& GetRows() const { return m_Rows; }
        void SetRows(std::vector<SampleTableRow> rows) { m_Rows = std::move(rows); }

    protected:
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetRows", Getter = "GetRows"))
        std::vector<SampleTableRow> m_Rows;
    };
}

#include "SampleTableDataAsset.gen.h"
