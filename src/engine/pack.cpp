#include "pack.h"

#include <algorithm>
#include <array>
#include <limits>

namespace tuxdan {

namespace {

constexpr int kMinMatch = 3;
constexpr int kMaxMatch = 129;
constexpr int kMaxLiterals = 128;
constexpr int kCandidates = 256;  // places tried for a match, nearest first
constexpr uint8_t kEnd = 0xFF;

}  // namespace

// Parsed for the fewest bytes: from the end back, each place takes the best
// of a match of any length up to the longest found there, or a run of bytes
// as they are.
std::vector<uint8_t> pack(std::span<const uint8_t> block)
{
    const int n = static_cast<int>(std::min(block.size(), kBlockSize));
    // The longest match at each place, by chains of places that start with
    // the same three bytes.
    std::vector<int> longest(static_cast<size_t>(n), 0), distance(static_cast<size_t>(n), 0);
    std::vector<int> head(1 << 16, -1), previous(static_cast<size_t>(n), -1);
    const auto hash = [&](int i) { return (block[i] << 8 ^ block[i + 1] << 4 ^ block[i + 2]) & 0xFFFF; };
    for (int i = 0; i + kMinMatch <= n; ++i) {
        const int h = hash(i);
        int tries = kCandidates;
        for (int j = head[h]; j >= 0 && tries-- > 0; j = previous[static_cast<size_t>(j)]) {
            int length = 0;
            const int most = std::min(kMaxMatch, n - i);
            while (length < most && block[j + length] == block[i + length])
                ++length;
            if (length > longest[static_cast<size_t>(i)]) {
                longest[static_cast<size_t>(i)] = length;
                distance[static_cast<size_t>(i)] = i - j;
                if (length == most)
                    break;
            }
        }
        previous[static_cast<size_t>(i)] = head[h];
        head[h] = i;
    }

    constexpr int kNone = std::numeric_limits<int>::max() / 2;
    std::vector<int> cost(static_cast<size_t>(n) + 1, kNone), step(static_cast<size_t>(n) + 1, 0);
    cost[static_cast<size_t>(n)] = 0;
    for (int i = n - 1; i >= 0; --i) {
        int best = kNone, how = 0;
        for (int k = 1; k <= kMaxLiterals && i + k <= n; ++k)
            if (1 + k + cost[static_cast<size_t>(i + k)] < best) {
                best = 1 + k + cost[static_cast<size_t>(i + k)];
                how = -k;
            }
        for (int length = kMinMatch; length <= longest[static_cast<size_t>(i)]; ++length)
            if (3 + cost[static_cast<size_t>(i + length)] <= best) {
                best = 3 + cost[static_cast<size_t>(i + length)];
                how = length;
            }
        cost[static_cast<size_t>(i)] = best;
        step[static_cast<size_t>(i)] = how;
    }

    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(cost[0]) + 1);
    for (int i = 0; i < n;) {
        const int how = step[static_cast<size_t>(i)];
        if (how < 0) {
            out.push_back(static_cast<uint8_t>(-how - 1));
            out.insert(out.end(), block.begin() + i, block.begin() + i - how);
            i -= how;
        } else {
            const int d = distance[static_cast<size_t>(i)];
            out.push_back(static_cast<uint8_t>(0x80 + how - kMinMatch));
            out.push_back(static_cast<uint8_t>(d));
            out.push_back(static_cast<uint8_t>(d >> 8));
            i += how;
        }
    }
    out.push_back(kEnd);
    return out;
}

std::optional<std::vector<uint8_t>> unpack(std::span<const uint8_t> in, size_t* used)
{
    std::vector<uint8_t> out;
    out.reserve(kBlockSize);
    size_t i = 0;
    while (i < in.size()) {
        const uint8_t token = in[i++];
        if (token == kEnd) {
            if (used)
                *used = i;
            return out;
        }
        if (token < 0x80) {
            const size_t count = token + 1u;
            if (i + count > in.size() || out.size() + count > kBlockSize)
                return std::nullopt;
            out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(i), in.begin() + static_cast<std::ptrdiff_t>(i + count));
            i += count;
        } else {
            if (i + 2 > in.size())
                return std::nullopt;
            const size_t length = token - 0x80u + kMinMatch;
            const size_t d = in[i] | in[i + 1] << 8;
            i += 2;
            if (d == 0 || d > out.size() || out.size() + length > kBlockSize)
                return std::nullopt;
            for (size_t k = 0; k < length; ++k)
                out.push_back(out[out.size() - d]);
        }
    }
    return std::nullopt;
}

}  // namespace tuxdan
