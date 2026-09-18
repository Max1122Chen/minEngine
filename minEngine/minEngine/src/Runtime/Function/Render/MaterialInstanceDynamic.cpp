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

        std::shared_ptr<Material> materialParent = std::dynamic_pointer_cast<Material>(parent);
        if (!materialParent)
        {
            if (MaterialInstance* parentInstance = dynamic_cast<MaterialInstance*>(parent.get()))
            {
                materialParent = parentInstance->GetParentShared();
            }
        }

        if (!materialParent)
        {
            return nullptr;
        }

        std::shared_ptr<MaterialInstanceDynamic> mid(new MaterialInstanceDynamic());
        if (!mid->InitializeParent(std::move(materialParent)))
        {
            return nullptr;
        }

        if (!debugName.empty())
        {
            mid->SetName(debugName);
        }

        return mid;
    }
}
