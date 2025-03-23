#pragma once

#include <climits>
#include <cstdint>
#include <vector>

class DynamicBitset {
private:
    // Use uint64_t for maximum performance on 64-bit architectures
    using BlockType = uint64_t;
    static constexpr size_t BITS_PER_BLOCK = sizeof(BlockType) * CHAR_BIT;

    std::vector<BlockType> mBlocks;
    size_t mNumBits;

    // Calculate which block a bit is in
    inline size_t block_index(size_t pos) const { return pos / BITS_PER_BLOCK; }

    // Calculate bit position within a block
    inline size_t bit_index(size_t pos) const { return pos % BITS_PER_BLOCK; }

    // Create mask for a single bit
    inline BlockType bit_mask(size_t pos) const { return BlockType(1) << bit_index(pos); }

public:
    // Default constructor creates empty bitset
    DynamicBitset();

    // Constructor with size, all bits initialized to zero
    explicit DynamicBitset(size_t size);

    // Resize the bitset
    void resize(size_t new_size);

    // Get size of bitset
    size_t size() const { return mNumBits; }

    // Set a bit to 1 (no bounds checking for speed)
    void set(size_t pos) { mBlocks[block_index(pos)] |= bit_mask(pos); }

    // Set a bit to a specific value (no bounds checking for speed)
    void set(size_t pos, bool value) {
        if (value) {
            set(pos);
        } else {
            mBlocks[block_index(pos)] &= ~bit_mask(pos);
        }
    }

    // Test if a bit is set (no bounds checking for speed)
    bool test(size_t pos) const { return (mBlocks[block_index(pos)] & bit_mask(pos)) != 0; }

    // Fast access operator (no bounds checking for speed)
    bool operator[](size_t pos) const { return test(pos); }

    // Bitwise AND operation
    DynamicBitset& operator&=(const DynamicBitset& other) {
        size_t min_blocks = std::min(mBlocks.size(), other.mBlocks.size());
        for (size_t i = 0; i < min_blocks; ++i) {
            mBlocks[i] &= other.mBlocks[i];
        }
        return *this;
    }

    // Bitwise OR operation
    DynamicBitset& operator|=(const DynamicBitset& other) {
        size_t min_blocks = std::min(mBlocks.size(), other.mBlocks.size());
        for (size_t i = 0; i < min_blocks; ++i) {
            mBlocks[i] |= other.mBlocks[i];
        }
        return *this;
    }

    // Bitwise XOR operation
    DynamicBitset& operator^=(const DynamicBitset& other) {
        size_t min_blocks = std::min(mBlocks.size(), other.mBlocks.size());
        for (size_t i = 0; i < min_blocks; ++i) {
            mBlocks[i] ^= other.mBlocks[i];
        }
        return *this;
    }

    // Bitwise NOT operation
    DynamicBitset operator~() const {
        DynamicBitset result(*this);
        for (size_t i = 0; i < result.mBlocks.size(); ++i) {
            result.mBlocks[i] = ~result.mBlocks[i];
        }
        return result;
    }

    // Binary operators implemented via compound assignment
    friend DynamicBitset operator&(const DynamicBitset& lhs, const DynamicBitset& rhs) {
        DynamicBitset result(lhs);
        result &= rhs;
        return result;
    }

    friend DynamicBitset operator|(const DynamicBitset& lhs, const DynamicBitset& rhs) {
        DynamicBitset result(lhs);
        result |= rhs;
        return result;
    }

    friend DynamicBitset operator^(const DynamicBitset& lhs, const DynamicBitset& rhs) {
        DynamicBitset result(lhs);
        result ^= rhs;
        return result;
    }

    // Equality operator
    bool operator==(const DynamicBitset& other) const {
        // Different sizes means they can't be equal
        if (mNumBits != other.mNumBits) {
            return false;
        }

        // Check all complete blocks for equality
        size_t full_blocks = mBlocks.size() - 1;
        for (size_t i = 0; i < full_blocks; ++i) {
            if (mBlocks[i] != other.mBlocks[i]) {
                return false;
            }
        }

        // For the last block, mask out unused bits before comparing
        if (mBlocks.size() > 0) {
            size_t used_bits_in_last_block = mNumBits % BITS_PER_BLOCK;
            if (used_bits_in_last_block == 0) {
                // Last block is full, simple comparison
                return mBlocks.back() == other.mBlocks.back();
            } else {
                // Create mask for used bits in last block
                BlockType mask = (BlockType(1) << used_bits_in_last_block) - 1;
                return (mBlocks.back() & mask) == (other.mBlocks.back() & mask);
            }
        }

        // If we get here, both are empty
        return true;
    }

    // Inequality operator
    bool operator!=(const DynamicBitset& other) const { return !(*this == other); }

    // Test if all bits are zero
    bool all_zero() const {
        // Check all complete mBlocks first
        for (size_t i = 0; i < mBlocks.size() - 1; ++i) {
            if (mBlocks[i] != 0) {
                return false;
            }
        }

        // For the last block, only check the bits that are part of the bitset
        if (mBlocks.size() > 0) {
            size_t used_bits_in_last_block = mNumBits % BITS_PER_BLOCK;
            if (used_bits_in_last_block == 0) {
                // Last block is fully used, just check if it's zero
                return mBlocks.back() == 0;
            } else {
                // Create mask for used bits in last block
                BlockType mask = (BlockType(1) << used_bits_in_last_block) - 1;
                return (mBlocks.back() & mask) == 0;
            }
        }

        // Empty bitset is considered all zero
        return true;
    }

    // Reset all bits to 0
    void reset() {
        for (size_t i = 0; i < mBlocks.size(); ++i) {
            mBlocks[i] = 0;
        }
    }

    // Reset a specific bit to 0 (no bounds checking for speed)
    void reset(size_t pos) { mBlocks[block_index(pos)] &= ~bit_mask(pos); }

    // Count the number of bits set to 1
    size_t count() const;
};
