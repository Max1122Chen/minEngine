#pragma once

namespace minEngine::Reflection
{
	template<typename T>
	struct FieldAccessor;
}

namespace minEngine::MEReflection
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
#define ME_REFLECT_FRIEND(TYPE) \
	template<typename T> friend struct ::minEngine::Reflection::FieldAccessor; \
	template<typename T> friend struct ::minEngine::MEReflection::FieldAccessor;
