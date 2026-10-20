#pragma once

namespace minEngine
{
    // BUILD-F03 knife 1: glad left the engine export table, so the engine can no longer inherit a
    // GL function table that a consumer loaded for it. glad's pointers are PER MODULE: the copy in
    // this DLL is initialized only by a gladLoadGLLoader call inside this DLL.
    //
    // CONTRACT: engine code that touches a glad global (any gl* call, glad_gl* pointer) must pass
    // through here first, unless it already runs behind OpenGLRHI::Initialize() or behind a GL
    // resource constructor - the two structural entries this guard is installed at. A new engine
    // GL entry point that is reachable without either must call this too, or it dereferences a
    // null function pointer whenever the consumer loaded its own glad copy instead.
    //
    // Idempotent. No-op (returns false) when no GL context is current, exactly the situation in
    // which the following gl* call was already invalid before this guard existed. Main thread
    // only, like every other GL entry in this engine: GLFW's "current context" is per thread.
    bool EnsureOpenGLFunctionTableLoaded();
}
