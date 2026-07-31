sol2 (header-only Lua bindings)

Pinned commit: d805d027e0a0a7222e936926139f06e23828ce9f
Upstream: https://github.com/ThePhD/sol2

Why not v3.3.1: GCC 15 / Clang 19 fail on optional<T&>::emplace
(see ThePhD/sol2#1695). This commit includes the emplace fix.

Only include/ is vendored; engine CMake adds Third-Party/sol2/include.
