#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace tuxdan {

// A CPC snapshot (.sna, versions 1 to 3) as a cartridge holds it: the
// header of the file, which says what the Z80 and the chips were doing, and
// the RAM, 64K or 128K.
struct Snapshot {
    static constexpr size_t kHeaderSize = 0x100;
    static constexpr size_t kBank = 0x10000;

    std::array<uint8_t, kHeaderSize> header = {};
    std::vector<uint8_t> ram;  // 64K, or 128K with the 6128's second bank

    // Offsets in the header.
    enum : size_t {
        kVersion = 0x10,
        kRegisters = 0x11,  // F A C B E D L H R I IFF1 IFF2 IXl IXh IYl IYh SPl SPh PCl PCh IM F' A' C' B' E' D' L' H'
        kSp = 0x21,
        kPc = 0x23,
        kIm = 0x25,
        kRamConfig = 0x41,
        kDumpSize = 0x6B,
        kMachine = 0x6D,
    };

    uint16_t word(size_t at) const { return static_cast<uint16_t>(header[at] | header[at + 1] << 8); }
    uint16_t pc() const { return word(kPc); }
    uint16_t sp() const { return word(kSp); }
    // The machine it was taken on: 0 a 464, 1 a 664, 2 a 6128; -1 not said.
    int machine() const { return header[kVersion] >= 2 && header[kMachine] <= 2 ? header[kMachine] : -1; }
    bool operator==(const Snapshot&) const = default;
};

// A snapshot file read, with its memory in chunks or not. Nothing, with the
// reason, if it is not one, or not one a cartridge can hold (a Plus's, or
// one with more than 128K).
std::optional<Snapshot> readSnapshot(std::span<const uint8_t> file, std::string* error = nullptr);

// The snapshot as a file: version 3, its memory as it is.
std::vector<uint8_t> writeSnapshot(const Snapshot& snapshot);

}  // namespace tuxdan
