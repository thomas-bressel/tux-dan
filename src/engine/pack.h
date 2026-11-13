#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace tuxdan {

// The packing of a block of 16K, as the cartridge's loader unpacks it (see
// z80/cartridge.asm): tokens of 0 to &7F for 1 to 128 bytes that follow as
// they are; &80 to &FE for 3 to 129 bytes found again further back in the
// block, the distance (1 to 16384) in the two bytes that follow, low byte
// first; &FF at the end.
constexpr size_t kBlockSize = 0x4000;

std::vector<uint8_t> pack(std::span<const uint8_t> block);

// The block back, and how many bytes of `in` it took. Nothing if `in` is
// not a block packed so.
std::optional<std::vector<uint8_t>> unpack(std::span<const uint8_t> in, size_t* used = nullptr);

}  // namespace tuxdan
