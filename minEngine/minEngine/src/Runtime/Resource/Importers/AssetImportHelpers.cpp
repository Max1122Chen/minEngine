#include "Runtime/Resource/Importers/AssetImportHelpers.h"

namespace minEngine
{
    ImportResult AssetImportHelpers::MakeResultFromLegacy(const ImportAssetResult& legacy)
    {
        ImportResult result;
        result.bSuccess = legacy.bSuccess;
        result.ErrorMessage = legacy.ErrorMessage;
        if (legacy.bSuccess && !legacy.Meta.AssetPath.empty())
        {
            result.Created.push_back(ImportCreatedAsset{
                .AssetPath = legacy.Meta.AssetPath,
                .AssetTypeId = legacy.Meta.AssetType,
                .Guid = legacy.Meta.Guid});
        }

        return result;
    }
}
