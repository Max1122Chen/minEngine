#pragma once

#include "MEFunction.h"

#include <cstring>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace minEngine
{
    class MEObject;
}

namespace minEngine::Reflection
{
    namespace Detail
    {
        inline const uint8_t* BytePtr(const void* ptr) { return static_cast<const uint8_t*>(ptr); }
        inline uint8_t* BytePtr(void* ptr) { return static_cast<uint8_t*>(ptr); }

        inline const MEParamDescriptor* FindReturnParam(const MEFunction& function)
        {
            for (const MEParamDescriptor& p : function.GetParams())
            {
                if (p.IsReturn())
                {
                    return &p;
                }
            }
            return nullptr;
        }

        inline std::vector<const MEParamDescriptor*> CollectArgParams(const MEFunction& function)
        {
            std::vector<const MEParamDescriptor*> args;
            args.reserve(function.GetParams().size());
            for (const MEParamDescriptor& p : function.GetParams())
            {
                if (p.IsReturn())
                {
                    continue;
                }
                args.push_back(&p);
            }
            return args;
        }

        template<typename T>
        using RemoveCVRef = std::remove_cv_t<std::remove_reference_t<T>>;

        template<typename T>
        inline void ReadValueFromSlot(const MEParamDescriptor& param, const void* parms, RemoveCVRef<T>& outValue)
        {
            const auto* src = BytePtr(parms) + param.Offset;
            static_assert(std::is_trivially_copyable_v<RemoveCVRef<T>>,
                          "Non-trivial value params are not supported in this invoke path (use ConstRef/Ref/Out).");
            std::memcpy(&outValue, src, sizeof(RemoveCVRef<T>));
        }

        template<typename T>
        inline void WriteValueToSlot(const MEParamDescriptor& param, void* parms, const RemoveCVRef<T>& value)
        {
            auto* dst = BytePtr(parms) + param.Offset;
            std::memcpy(dst, &value, sizeof(RemoveCVRef<T>));
        }

        template<typename T>
        inline void* ReadPtrFromSlot(const MEParamDescriptor& param, void* parms)
        {
            void* ptr = nullptr;
            std::memcpy(&ptr, BytePtr(parms) + param.Offset, sizeof(void*));
            return ptr;
        }

        template<typename T>
        inline const void* ReadConstPtrFromSlot(const MEParamDescriptor& param, const void* parms)
        {
            const void* ptr = nullptr;
            std::memcpy(&ptr, BytePtr(parms) + param.Offset, sizeof(const void*));
            return ptr;
        }

        template<typename TArg>
        struct CallArgHolder
        {
            using TRaw = RemoveCVRef<TArg>;
            TRaw Value{};
            const TRaw* ValuePtr = nullptr;
            TRaw* Ptr = nullptr;
            const TRaw* ConstPtr = nullptr;
            bool Ok = true;

            static CallArgHolder Load(const MEParamDescriptor& param, void* parms)
            {
                CallArgHolder holder;

                if constexpr (std::is_lvalue_reference_v<TArg>)
                {
                    if constexpr (std::is_const_v<std::remove_reference_t<TArg>>)
                    {
                        const void* ptr = ReadConstPtrFromSlot<TRaw>(param, parms);
                        holder.ConstPtr = static_cast<const TRaw*>(ptr);
                        holder.Ok = holder.ConstPtr != nullptr;
                    }
                    else
                    {
                        void* ptr = ReadPtrFromSlot<TRaw>(param, parms);
                        holder.Ptr = static_cast<TRaw*>(ptr);
                        holder.Ok = holder.Ptr != nullptr;
                    }
                }
                else
                {
                    if constexpr (std::is_trivially_copyable_v<TRaw>)
                    {
                        ReadValueFromSlot<TArg>(param, parms, holder.Value);
                    }
                    else
                    {
                        holder.ValuePtr = reinterpret_cast<const TRaw*>(BytePtr(parms) + param.Offset);
                        holder.Ok = holder.ValuePtr != nullptr;
                    }
                }

                return holder;
            }

            decltype(auto) Get()
            {
                if constexpr (std::is_lvalue_reference_v<TArg>)
                {
                    if constexpr (std::is_const_v<std::remove_reference_t<TArg>>)
                    {
                        return *ConstPtr;
                    }
                    else
                    {
                        return *Ptr;
                    }
                }
                else
                {
                    if constexpr (std::is_trivially_copyable_v<TRaw>)
                    {
                        return static_cast<TArg>(Value);
                    }
                    else
                    {
                        return static_cast<TArg>(*ValuePtr);
                    }
                }
            }
        };

        template<typename... TArgs, size_t... Indices>
        inline std::tuple<CallArgHolder<TArgs>...> LoadArgHoldersTuple(const std::vector<const MEParamDescriptor*>& params,
                                                                       void* parms, std::index_sequence<Indices...>)
        {
            return std::tuple<CallArgHolder<TArgs>...>{ CallArgHolder<TArgs>::Load(*params[Indices], parms)... };
        }

        template<typename TReturn>
        inline void StoreReturnValue(const MEFunction& function, void* parms, const TReturn& value)
        {
            const MEParamDescriptor* ret = FindReturnParam(function);
            if (ret == nullptr)
            {
                return;
            }

            MEProperty* returnProperty = ret->Property;
            if (returnProperty != nullptr && returnProperty->GetValueCopyAssignFn() != nullptr)
            {
                void* dst = BytePtr(parms) + ret->Offset;
                returnProperty->CopyAssignValue(dst, &value);
                return;
            }

            WriteValueToSlot<TReturn>(*ret, parms, value);
        }
    } // namespace Detail

    template<typename TOwner, typename MethodT, MethodT TMethod>
    struct NativeThunkInvoker;

    template<typename TOwner, typename R, typename... TArgs, R (TOwner::*TMethod)(TArgs...)>
    struct NativeThunkInvoker<TOwner, R (TOwner::*)(TArgs...), TMethod>
    {
        static void Invoke(minEngine::MEObject* context, MEFunction* function, void* parms)
        {
            if (context == nullptr || function == nullptr)
            {
                return;
            }

            auto* owner = static_cast<TOwner*>(context);

            const std::vector<const MEParamDescriptor*> args = Detail::CollectArgParams(*function);
            if (args.size() != sizeof...(TArgs))
            {
                return;
            }

            auto holders =
                Detail::LoadArgHoldersTuple<TArgs...>(args, parms, std::make_index_sequence<sizeof...(TArgs)>{});

            const bool ok = std::apply([](auto&... h) { return (h.Ok && ...); }, holders);
            if (!ok)
            {
                return;
            }

            if constexpr (std::is_void_v<R>)
            {
                std::apply(
                    [&](auto&... h)
                    {
                        (owner->*TMethod)(h.Get()...);
                    },
                    holders);
            }
            else
            {
                R ret = std::apply(
                    [&](auto&... h)
                    {
                        return (owner->*TMethod)(h.Get()...);
                    },
                    holders);
                Detail::StoreReturnValue(*function, parms, ret);
            }
        }
    };

    template<typename TOwner, typename R, typename... TArgs, R (*TMethod)(TArgs...)>
    struct NativeThunkInvoker<TOwner, R (*)(TArgs...), TMethod>
    {
        static void Invoke(minEngine::MEObject* /*context*/, MEFunction* function, void* parms)
        {
            if (function == nullptr)
            {
                return;
            }

            const std::vector<const MEParamDescriptor*> args = Detail::CollectArgParams(*function);
            if (args.size() != sizeof...(TArgs))
            {
                return;
            }

            auto holders =
                Detail::LoadArgHoldersTuple<TArgs...>(args, parms, std::make_index_sequence<sizeof...(TArgs)>{});

            const bool ok = std::apply([](auto&... h) { return (h.Ok && ...); }, holders);
            if (!ok)
            {
                return;
            }

            if constexpr (std::is_void_v<R>)
            {
                std::apply(
                    [&](auto&... h)
                    {
                        (*TMethod)(h.Get()...);
                    },
                    holders);
            }
            else
            {
                R ret = std::apply(
                    [&](auto&... h)
                    {
                        return (*TMethod)(h.Get()...);
                    },
                    holders);
                Detail::StoreReturnValue(*function, parms, ret);
            }
        }
    };

    template<typename TOwner, typename R, typename... TArgs, R (TOwner::*TMethod)(TArgs...) const>
    struct NativeThunkInvoker<TOwner, R (TOwner::*)(TArgs...) const, TMethod>
    {
        static void Invoke(minEngine::MEObject* context, MEFunction* function, void* parms)
        {
            if (context == nullptr || function == nullptr)
            {
                return;
            }

            const auto* owner = static_cast<const TOwner*>(context);

            const std::vector<const MEParamDescriptor*> args = Detail::CollectArgParams(*function);
            if (args.size() != sizeof...(TArgs))
            {
                return;
            }

            auto holders =
                Detail::LoadArgHoldersTuple<TArgs...>(args, parms, std::make_index_sequence<sizeof...(TArgs)>{});

            const bool ok = std::apply([](auto&... h) { return (h.Ok && ...); }, holders);
            if (!ok)
            {
                return;
            }

            if constexpr (std::is_void_v<R>)
            {
                std::apply(
                    [&](auto&... h)
                    {
                        (owner->*TMethod)(h.Get()...);
                    },
                    holders);
            }
            else
            {
                R ret = std::apply(
                    [&](auto&... h)
                    {
                        return (owner->*TMethod)(h.Get()...);
                    },
                    holders);
                Detail::StoreReturnValue(*function, parms, ret);
            }
        }
    };

    template<typename TOwner, auto TMethod>
    void InvokeNativeThunk(minEngine::MEObject* context, MEFunction* function, void* parms)
    {
        NativeThunkInvoker<TOwner, decltype(TMethod), TMethod>::Invoke(context, function, parms);
    }

} // namespace minEngine::Reflection

