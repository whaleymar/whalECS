#include "DynamicBitset.h"

DynamicBitset::DynamicBitset() : mNumBits(0) {}

DynamicBitset::DynamicBitset(size_t size) : mBlocks(size / BITS_PER_BLOCK + (size % BITS_PER_BLOCK ? 1 : 0), 0), mNumBits(size) {}

void DynamicBitset::resize(size_t new_size) {
    size_t new_block_count = new_size / BITS_PER_BLOCK + (new_size % BITS_PER_BLOCK ? 1 : 0);
    mBlocks.resize(new_block_count, 0);
    mNumBits = new_size;
}

size_t DynamicBitset::count() const {
    size_t result = 0;

    // Count bits in all complete blocks
    for (size_t i = 0; i < mBlocks.size(); ++i) {
// Use built-in population count if available (C++20)
#if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
        result += std::popcount(mBlocks[i]);
// Otherwise use compiler intrinsics if available
#elif defined(__GNUC__) || defined(__clang__)
        result += __builtin_popcountll(mBlocks[i]);
// Fallback to manual counting
#else
        BlockType block = mBlocks[i];
        while (block) {
            result += block & 1;
            block >>= 1;
        }
#endif
    }

    return result;
}
