#include "MEObject.h"
#include "ObjectManager.h"

namespace minEngine
{
    MEObject::~MEObject()
    {
        if (m_Guid.IsZero() || !ObjectManager::HasInstance())
        {
            return;
        }

        ObjectManager::Get().UnregisterObject(m_Guid);
    }
}
