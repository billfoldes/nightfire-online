// LZ4 (third_party/lz4, BSD 2-Clause), built as part of the runtime: snapshots travel compressed
#include "../../third_party/lz4/lz4.c"

#include <cstring>

#include "core/snapshot.h"

namespace snapshot {

std::vector<uint8_t> Compress(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> out(8 + size_t(LZ4_compressBound(int(raw.size()))));
    uint64_t n = raw.size();
    std::memcpy(out.data(), &n, 8);
    int m = LZ4_compress_default(reinterpret_cast<const char*>(raw.data()), reinterpret_cast<char*>(out.data() + 8),
                                 int(raw.size()), int(out.size() - 8));
    out.resize(m > 0 ? 8 + size_t(m) : 0);
    return out;
}

bool Decompress(const uint8_t* p, size_t n, std::vector<uint8_t>& raw) {
    uint64_t size;
    if (n < 8 || (std::memcpy(&size, p, 8), size > (1ull << 31))) return false;
    raw.resize(size_t(size));
    int m = LZ4_decompress_safe(reinterpret_cast<const char*>(p + 8), reinterpret_cast<char*>(raw.data()), int(n - 8),
                                int(size));
    return m >= 0 && uint64_t(m) == size;
}

}  // namespace snapshot
