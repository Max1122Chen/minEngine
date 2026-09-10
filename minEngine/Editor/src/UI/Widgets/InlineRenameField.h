#pragma once

#include <cstddef>

namespace minEngine
{
    /**
     * Shared inline rename InputText for Hierarchy / Inspector / Content Browser.
     * Enter = Commit (empty or whitespace = Cancel). Esc or click-away / blur = Cancel.
     */
    class InlineRenameField
    {
    public:
        enum class Result
        {
            Editing,
            Commit,
            Cancel
        };

        /**
         * @param buffer Mutable UTF-8 name buffer (null-terminated).
         * @param bufferSize Total size of buffer including null terminator.
         * @param requestFocus Set true when entering rename; cleared after focus is applied.
         */
        static Result Draw(char* buffer, size_t bufferSize, bool& requestFocus);

    private:
        static bool IsBlankName(const char* buffer);
    };
}
