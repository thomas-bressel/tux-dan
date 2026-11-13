// The packing of 16K blocks: what is packed comes back as it was, for
// blocks of every kind, and is smaller where the block repeats itself; a
// damaged stream is refused.

#include <cstdint>
#include <random>
#include <vector>

#include "check.h"
#include "pack.h"

using namespace tuxdan;

namespace {

void roundTrip(const std::vector<uint8_t>& block, const char* what, size_t most)
{
    const auto packed = pack(block);
    size_t used = 0;
    const auto back = unpack(packed, &used);
    CHECK(back.has_value());
    if (back)
        CHECK(*back == block);
    CHECK_EQ(used, packed.size());
    CHECK_EQ(packed.back(), 0xFF);
    if (packed.size() > most)
        std::printf("%s: %zu bytes, more than %zu\n", what, packed.size(), most);
    CHECK(packed.size() <= most);
}

}  // namespace

int main()
{
    // All the same: a few matches of the longest.
    roundTrip(std::vector<uint8_t>(kBlockSize, 0), "zeros", 400);
    // Noise: bytes as they are, one token a 128.
    std::mt19937 random(1);
    std::vector<uint8_t> noise(kBlockSize);
    for (uint8_t& b : noise)
        b = static_cast<uint8_t>(random());
    roundTrip(noise, "noise", kBlockSize + kBlockSize / 128 + 2);
    // A picture's worth of patterns, with noise between them.
    std::vector<uint8_t> mixed(kBlockSize);
    for (size_t i = 0; i < mixed.size(); ++i)
        mixed[i] = i % 1000 < 600 ? static_cast<uint8_t>(i % 7 * 3) : static_cast<uint8_t>(random());
    roundTrip(mixed, "mixed", 8000);
    // A match as far back as can be, and runs that overlap themselves.
    std::vector<uint8_t> far(kBlockSize, 0x55);
    far[0] = 1;
    far[1] = 2;
    far[2] = 3;
    far[kBlockSize - 3] = 1;
    far[kBlockSize - 2] = 2;
    far[kBlockSize - 1] = 3;
    roundTrip(far, "far", 400);

    // Damaged: cut short, a distance before the start, too long.
    auto packed = pack(mixed);
    packed.resize(packed.size() / 2);
    CHECK(!unpack(packed).has_value());
    CHECK(!unpack(std::vector<uint8_t>{0x80, 0x05, 0x00, 0xFF}).has_value());
    std::vector<uint8_t> tooLong;
    for (int i = 0; i < 200; ++i)
        tooLong.insert(tooLong.end(), {0x00, 0x00, 0xFE, 0x01, 0x00});
    tooLong.push_back(0xFF);
    CHECK(!unpack(tooLong).has_value());
    return checkSummary("engine_pack");
}
