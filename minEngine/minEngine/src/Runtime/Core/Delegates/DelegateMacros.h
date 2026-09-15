#pragma once

#include "Runtime/Core/Delegates/DynamicMulticastDelegate.h"
#include "Runtime/Core/Delegates/MulticastDelegate.h"

// Convention: name declared aliases with D-prefix (e.g. DOnClicked), not UE-style F-prefix.

/// Declares a 0-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = ::minEngine::MulticastDelegate<>

/// Declares a 1-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, ParamType1) \
    using DelegateName = ::minEngine::MulticastDelegate<ParamType1>

/// Declares a 2-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, ParamType1, ParamType2) \
    using DelegateName = ::minEngine::MulticastDelegate<ParamType1, ParamType2>

/// Declares a 0-parameter dynamic multicast delegate type alias.
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = ::minEngine::DynamicMulticastDelegate<>

/// Declares a 1-parameter dynamic multicast delegate type alias.
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(DelegateName, ParamType1) \
    using DelegateName = ::minEngine::DynamicMulticastDelegate<ParamType1>

/// Declares a 2-parameter dynamic multicast delegate type alias.
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(DelegateName, ParamType1, ParamType2) \
    using DelegateName = ::minEngine::DynamicMulticastDelegate<ParamType1, ParamType2>
