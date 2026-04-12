#pragma once

namespace minEngine::Reflection
{
	template<typename T>
	struct FieldAccessor;
}

// Marker macros for lightweight reflection code generation.
// They are intentionally empty and only consumed by the Python generator.
#define ME_CLASS(...)
#define ME_STRUCT(...)
#define ME_ENUM(...)
#define ME_PROPERTY(...)
#define ME_REFLECTION_FRIEND(TYPE) \
	template<typename T> friend struct ::minEngine::Reflection::FieldAccessor;
