#include "MaterialInstanceDynamic.h"

#include "Runtime/Function/Render/Material.h"

namespace minEngine
{
    std::shared_ptr<MaterialInstanceDynamic> MaterialInstanceDynamic::Create(
        std::shared_ptr<MaterialInterface> parent,
        const std::string& debugName)
    {
        if (!parent)
        {
            return nullptr;
        }

        std::shared_ptr<Material> materialParent = parent->ResolveParentMaterial(parent);
        if (!materialParent)
        {
            return nullptr;
        }

        std::shared_ptr<MaterialInstanceDynamic> mid(new MaterialInstanceDynamic());
        if (!mid->InitializeParent(std::move(materialParent)))
        {
            return nullptr;
        }

        // MID is not a separate ME_CLASS yet; identity as MaterialInstance for IsA / tools.
        mid->SetClass(MaterialInstance::StaticClass());

        if (!debugName.empty())
        {
            mid->SetName(debugName);
        }

        return mid;
    }
}
