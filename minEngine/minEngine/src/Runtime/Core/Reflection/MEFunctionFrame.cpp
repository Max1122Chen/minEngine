#include "MEFunctionFrame.h"

#include "MEFunction.h"

namespace minEngine::Reflection
{
    MEFunctionFrame::MEFunctionFrame(const MEFunction& function)
        : m_Function(function)
    {
        m_Buffer.resize(function.GetParmsSize(), 0);
    }

    MEFunctionFrame::~MEFunctionFrame() = default;

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
        return m_Function.CopyParamToBuffer(GetBuffer(), name, value, valueSize);
    }

    bool MEFunctionFrame::GetParam(const std::string& name, void* outValue, size_t outValueSize) const
    {
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
