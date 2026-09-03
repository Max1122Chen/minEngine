#pragma once

// LLVM libc++ does not define std::char_traits<unsigned int>, which efsw's
// Utf32 string typedef needs. Used only for ASan builds with clang++/libc++.

#include <cstring>
#include <cwchar>
#include <string>

namespace std
{
    template <>
    struct char_traits<unsigned int>
    {
        using char_type = unsigned int;
        using int_type = unsigned int;
        using off_type = streamoff;
        using pos_type = streampos;
        using state_type = mbstate_t;

        static void assign(char_type& c1, const char_type& c2) noexcept { c1 = c2; }
        static constexpr bool eq(char_type c1, char_type c2) noexcept { return c1 == c2; }
        static constexpr bool lt(char_type c1, char_type c2) noexcept { return c1 < c2; }

        static int compare(const char_type* s1, const char_type* s2, size_t n)
        {
            for (size_t i = 0; i < n; ++i)
            {
                if (lt(s1[i], s2[i]))
                {
                    return -1;
                }
                if (lt(s2[i], s1[i]))
                {
                    return 1;
                }
            }
            return 0;
        }

        static size_t length(const char_type* s)
        {
            size_t n = 0;
            while (!eq(s[n], char_type()))
            {
                ++n;
            }
            return n;
        }

        static const char_type* find(const char_type* s, size_t n, const char_type& a)
        {
            for (size_t i = 0; i < n; ++i)
            {
                if (eq(s[i], a))
                {
                    return s + i;
                }
            }
            return nullptr;
        }

        static char_type* move(char_type* s1, const char_type* s2, size_t n)
        {
            return static_cast<char_type*>(::memmove(s1, s2, n * sizeof(char_type)));
        }

        static char_type* copy(char_type* s1, const char_type* s2, size_t n)
        {
            return static_cast<char_type*>(::memcpy(s1, s2, n * sizeof(char_type)));
        }

        static char_type* assign(char_type* s, size_t n, char_type a)
        {
            for (size_t i = 0; i < n; ++i)
            {
                s[i] = a;
            }
            return s;
        }

        static constexpr int_type not_eof(int_type c) noexcept
        {
            return eq_int_type(c, eof()) ? ~eof() : c;
        }

        static constexpr char_type to_char_type(int_type c) noexcept
        {
            return static_cast<char_type>(c);
        }

        static constexpr int_type to_int_type(char_type c) noexcept
        {
            return static_cast<int_type>(c);
        }

        static constexpr bool eq_int_type(int_type c1, int_type c2) noexcept
        {
            return c1 == c2;
        }

        static constexpr int_type eof() noexcept
        {
            return static_cast<int_type>(-1);
        }
    };
}
