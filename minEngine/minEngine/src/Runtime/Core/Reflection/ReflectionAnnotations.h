#pragma once

#include "EngineAPI.h"

#ifndef ME_REFLECTION_MODULE_API
#	define ME_REFLECTION_MODULE_API MINENGINE_API
#endif

namespace minEngine::Reflection
{
	template<typename T>
	struct FieldAccessor;

	class MEClass;
}


// Marker macros for lightweight reflection code generation.
// They are intentionally empty and only consumed by the Python generator.
#define ME_CLASS(...)
#define ME_STRUCT(...)
#define ME_ENUM(...)
#define ME_PROPERTY(...)
#define ME_FUNCTION(...)
#define ME_GENERATED_BODY() \
	template<typename T> friend struct ::minEngine::Reflection::FieldAccessor; \
public: \
	static ME_REFLECTION_MODULE_API const minEngine::Reflection::MEClass* StaticClass();
