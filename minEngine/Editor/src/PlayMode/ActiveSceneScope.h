#pragma once

#include "Core.h"

namespace minEngine
{
    class Scene;

    class ActiveSceneScope
    {
    public:
        explicit ActiveSceneScope(Scene* pieScene);
        ~ActiveSceneScope();

        ActiveSceneScope(const ActiveSceneScope&) = delete;
        ActiveSceneScope& operator=(const ActiveSceneScope&) = delete;

    private:
        Scene* m_PreviousScene = nullptr;
    };
}
