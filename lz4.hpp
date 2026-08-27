#pragma once

#include "lz4.h"

namespace lz4 {

/**
 * Decompress LZ4 block data.
 * @param src Input compressed buffer
 * @param dst Output decompressed buffer
 * @param src_len Compressed data length in bytes
 * @param dst_len Max decompressed buffer capacity in bytes
 * @return Number of decompressed bytes written, or negative value on error.
 */
inline int decompress(const uint8_t* src, uint8_t* dst, int src_len, int dst_len) {
    return LZ4_decompress_safe(reinterpret_cast<const char*>(src),
                               reinterpret_cast<char*>(dst),
                               src_len, dst_len);
}

} // namespace lz4
