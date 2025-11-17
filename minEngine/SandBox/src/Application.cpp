#include "minEngine.h"

class EditorApplication : public minEngine::Application
{
public:
    EditorApplication()
    {}

    ~EditorApplication()
    {}
};

minEngine::Application* minEngine::CreateApplication()
{
    return new EditorApplication();
}
