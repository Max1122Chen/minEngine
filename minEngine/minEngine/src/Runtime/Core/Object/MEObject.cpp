#include "MEObject.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Reflection/MEFunction.h"
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

    bool MEObject::InvokeFunction(Reflection::MEFunction* function, void* parmsBuffer)
    {
        if (function == nullptr)
        {
            ME_CORE_ERROR("MEObject::InvokeFunction: null function.");
            return false;
        }

        if (parmsBuffer == nullptr && function->GetParmsSize() > 0)
        {
            ME_CORE_ERROR("MEObject::InvokeFunction: null parms buffer for '{}'.", function->GetName());
            return false;
        }

        if (!function->IsStatic())
        {
            if (m_Class == nullptr || !IsA(function->GetOwnerClass()))
            {
                ME_CORE_ERROR("MEObject::InvokeFunction: IsA mismatch for '{}'.", function->GetName());
                return false;
            }
        }

        const Reflection::MENativeThunkFn nativeThunk = function->GetNativeThunk();
        if (nativeThunk == nullptr)
        {
            ME_CORE_ERROR("MEObject::InvokeFunction: no native thunk for '{}'.", function->GetName());
            return false;
        }

        nativeThunk(this, parmsBuffer);
        return true;
    }

    bool MEObject::InvokeFunctionByName(const std::string& functionName, void* parmsBuffer)
    {
        if (m_Class == nullptr)
        {
            ME_CORE_ERROR("MEObject::InvokeFunctionByName: object has no class.");
            return false;
        }

        Reflection::MEFunction* function = m_Class->FindFunction(functionName);
        if (function == nullptr)
        {
            ME_CORE_ERROR("MEObject::InvokeFunctionByName: function '{}' not found.", functionName);
            return false;
        }

        return InvokeFunction(function, parmsBuffer);
    }
}
