#include "Runtime/Function/Framework/Prefab/Prefab.h"

#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    GameObject* Prefab::GetRootGameObject() const
    {
        if (m_RootGuid.IsZero())
        {
            return nullptr;
        }

        for (const std::shared_ptr<GameObject>& gameObject : m_TemplateObjects)
        {
            if (gameObject && gameObject->GetGuid() == m_RootGuid)
            {
                return gameObject.get();
            }
        }

        return nullptr;
    }

    void Prefab::ClearTemplateObjects()
    {
        m_TemplateObjects.clear();
        m_RootGuid = GUID::Zero();
    }

    void Prefab::AddTemplateObject(const std::shared_ptr<GameObject>& gameObject)
    {
        if (gameObject)
        {
            m_TemplateObjects.push_back(gameObject);
        }
    }

    bool Prefab::ValidateSingleRoot(std::string* outError) const
    {
        if (m_TemplateObjects.empty())
        {
            if (outError)
            {
                *outError = "Prefab has no template objects.";
            }
            return false;
        }

        GameObject* root = GetRootGameObject();
        if (root == nullptr)
        {
            if (outError)
            {
                *outError = "Prefab RootGuid does not match any template GameObject.";
            }
            return false;
        }

        if (root->GetParent() != nullptr)
        {
            if (outError)
            {
                *outError = "Prefab root GameObject must have null parent.";
            }
            return false;
        }

        size_t rootCount = 0;
        for (const std::shared_ptr<GameObject>& gameObject : m_TemplateObjects)
        {
            if (gameObject && gameObject->GetParent() == nullptr)
            {
                ++rootCount;
            }
        }

        if (rootCount != 1)
        {
            if (outError)
            {
                *outError = "Prefab must have exactly one parent-null GameObject root.";
            }
            return false;
        }

        return true;
    }
}
