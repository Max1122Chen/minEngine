#pragma once

namespace minEngine
{
    /** BUG-EDITOR-002: log unhandled AV + stack addresses to bin/ed_crash.log (Debug builds). */
    void InstallEditorCrashDiagnostics();
}
