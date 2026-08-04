#pragma once

#include "Runtime/Core/Delegates/MulticastDelegate.h"

/// Declares a 0-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE(DelegateName) \
    using DelegateName = ::minEngine::MulticastDelegate<>

/// Declares a 1-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE_OneParam(DelegateName, ParamType1) \
    using DelegateName = ::minEngine::MulticastDelegate<ParamType1>

/// Declares a 2-parameter multicast delegate type alias.
#define DECLARE_MULTICAST_DELEGATE_TwoParams(DelegateName, ParamType1, ParamType2) \
    using DelegateName = ::minEngine::MulticastDelegate<ParamType1, ParamType2>
