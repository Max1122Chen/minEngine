#pragma once

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
#define ME_GENERATED_BODY(TYPE) \
	template<typename T> friend struct ::minEngine::Reflection::FieldAccessor; \
	static const minEngine::Reflection::MEClass* StaticClass();
