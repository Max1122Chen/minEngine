#include "MEFunctionFrame.h"

#include "MEFunction.h"
#include <cstring>

namespace minEngine::Reflection
{
    MEFunctionFrame::MEFunctionFrame(const MEFunction& function)
        : m_Function(function)
    {
        m_Buffer.resize(function.GetParmsSize(), 0);

        for (const MEParamDescriptor& param : function.GetParams())
        {
            if (param.Property == nullptr || param.PassKind != MEParamPassKind::Value || param.IsOut())
            {
                continue;
            }

            if (param.Property->GetValueConstructFn() == nullptr || param.Property->GetValueDestructFn() == nullptr)
            {
                continue;
            }

            if (m_Buffer.empty())
            {
                continue;
            }

            void* dst = m_Buffer.data() + param.Offset;
            param.Property->ConstructValue(dst);
            m_LifetimeSlots.push_back(LifetimeSlot{ param.Property, param.Offset });
        }
    }

    MEFunctionFrame::~MEFunctionFrame()
    {
        for (auto iter = m_LifetimeSlots.rbegin(); iter != m_LifetimeSlots.rend(); ++iter)
        {
            if (iter->Property == nullptr || m_Buffer.empty())
            {
                continue;
            }

            void* dst = m_Buffer.data() + iter->Offset;
            iter->Property->DestructValue(dst);
        }
    }

    void* MEFunctionFrame::GetBuffer()
    {
        return m_Buffer.empty() ? nullptr : m_Buffer.data();
    }

    const void* MEFunctionFrame::GetBuffer() const
    {
        return m_Buffer.empty() ? nullptr : m_Buffer.data();
    }

    bool MEFunctionFrame::SetParam(const std::string& name, const void* value, size_t valueSize)
    {
        const MEParamDescriptor* param = m_Function.FindParam(name);
        if (param == nullptr || param->Property == nullptr || value == nullptr)
        {
            return false;
        }

        if (param->Role == MEParamRole::Out || param->PassKind == MEParamPassKind::Ref
            || param->PassKind == MEParamPassKind::ConstRef)
        {
            return m_Function.CopyParamToBuffer(GetBuffer(), name, value, valueSize);
        }

        if (param->Property->GetStorageSize() != valueSize || m_Buffer.empty())
        {
            return false;
        }

        if (param->Property->GetValueCopyAssignFn() != nullptr)
        {
            void* dst = m_Buffer.data() + param->Offset;
            param->Property->CopyAssignValue(dst, value);
            return true;
        }

        return m_Function.CopyParamToBuffer(GetBuffer(), name, value, valueSize);
    }

    bool MEFunctionFrame::GetParam(const std::string& name, void* outValue, size_t outValueSize) const
    {
        const MEParamDescriptor* param = m_Function.FindParam(name);
        if (param == nullptr || param->Property == nullptr || outValue == nullptr)
        {
            return false;
        }

        if (param->Role == MEParamRole::Out || param->PassKind == MEParamPassKind::Ref
            || param->PassKind == MEParamPassKind::ConstRef)
        {
            return m_Function.CopyParamFromBuffer(GetBuffer(), name, outValue, outValueSize);
        }

        if (param->Property->GetStorageSize() != outValueSize || m_Buffer.empty())
        {
            return false;
        }

        if (param->Property->GetValueCopyAssignFn() != nullptr)
        {
            const void* src = m_Buffer.data() + param->Offset;
            std::memcpy(outValue, src, outValueSize);
            return true;
        }

        return m_Function.CopyParamFromBuffer(GetBuffer(), name, outValue, outValueSize);
    }

    bool MEFunctionFrame::SetParamPtr(const std::string& name, void* ptr)
    {
        return SetParam(name, &ptr, sizeof(ptr));
    }

    bool MEFunctionFrame::SetParamConstPtr(const std::string& name, const void* ptr)
    {
        return SetParam(name, &ptr, sizeof(ptr));
    }

    bool MEFunctionFrame::GetParamPtr(const std::string& name, void*& outPtr) const
    {
        void* ptr = nullptr;
        if (!GetParam(name, &ptr, sizeof(ptr)))
        {
            return false;
        }
        outPtr = ptr;
        return true;
    }

    bool MEFunctionFrame::GetParamConstPtr(const std::string& name, const void*& outPtr) const
    {
        const void* ptr = nullptr;
        if (!GetParam(name, &ptr, sizeof(ptr)))
        {
            return false;
        }
        outPtr = ptr;
        return true;
    }

} // namespace minEngine::Reflection
