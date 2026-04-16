#include "GUID.h"
#include <random>

namespace minEngine
{
    // Using UUID v4 (random) for GUID generation
    GUID GenerateGUID()
    {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());

        std::uniform_int_distribution<uint64_t> dis;

        uint64_t high = dis(gen);
        uint64_t low = dis(gen);

        // Set version = 4
        high = (high & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;

        // Set variant = 10xx
        low = (low & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

        return GUID(high, low);
    }
}