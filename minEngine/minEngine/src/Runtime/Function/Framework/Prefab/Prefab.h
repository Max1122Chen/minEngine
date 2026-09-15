#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"
#include "Runtime/Resource/Asset.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class GameObject;

    /**
     * Prefab asset: a single-root GameObject template tree (not a Scene).
     * Disk: .meprefab + .meta
     */
    ME_CLASS()
    class Prefab : public Asset
    {
        ME_GENERATED_BODY()
    public:
        Prefab() = default;
        virtual ~Prefab() = default;

        GameObject* GetRootGameObject() const;
        const GUID& GetRootGuid() const { return m_RootGuid; }
        void SetRootGuid(const GUID& rootGuid) { m_RootGuid = rootGuid; }

        const std::vector<std::shared_ptr<GameObject>>& GetTemplateObjects() const { return m_TemplateObjects; }
        std::vector<std::shared_ptr<GameObject>>& GetTemplateObjectsMutable() { return m_TemplateObjects; }

        void ClearTemplateObjects();
        void AddTemplateObject(const std::shared_ptr<GameObject>& gameObject);
        bool ValidateSingleRoot(std::string* outError = nullptr) const;

    private:
        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<GameObject>> m_TemplateObjects;

        ME_PROPERTY()
        GUID m_RootGuid;
    };
}

#include "Prefab.gen.h"
