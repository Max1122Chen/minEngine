#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
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
    class MINENGINE_API Prefab : public Asset
    {
        ME_GENERATED_BODY()
    public:
        Prefab() = default;
        /**
         * A template tree keeps its parent GUID links but never rebuilds m_Children, so a child
         * destroyed after its parent would unlink itself through an already freed parent. Dropping
         * the parent links first removes that ordering hazard. A full detach is used rather than a
         * parent link clear because the template components are wired to each other through their
         * attach chain as well, and that chain has the same destroy-order problem.
         * Inline on purpose: the engine export contract is frozen, so a key function declared here
         * could not be reached from the consumers.
         */
        virtual ~Prefab()
        {
            for (const std::shared_ptr<GameObject>& templateObject : m_TemplateObjects)
            {
                if (templateObject)
                {
                    templateObject->DetachFromParent(AttachmentTransformRules::KeepWorldTransform);
                }
            }
        }

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
