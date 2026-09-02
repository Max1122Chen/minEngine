#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <unordered_map>

namespace minEngine
{
    class Component;
    class GameObject;
    struct SceneCloneContext;

    class PlayObjectMapping
    {
    public:
        void Build(const SceneCloneContext& cloneContext);
        void Clear();

        GameObject* FindPIECounterpart(const GameObject& editorGO) const;
        GameObject* FindEditorCounterpart(const GameObject& pieGO) const;
        Component* FindPIECounterpart(const Component& editorComp) const;

    private:
        std::unordered_map<GUID, GUID, GUID::Hash> m_EditorToPIE;
        std::unordered_map<GUID, GUID, GUID::Hash> m_PIEToEditor;
    };
}
