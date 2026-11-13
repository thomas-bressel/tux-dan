#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "snapshot.h"

namespace tuxdan {

// A game of the cartridge's menu: a snapshot, or a ROM of 16K put at &0000
// in place of the machine's (a test ROM, a game made as a ROM).
struct Game {
    enum class Kind : uint8_t { Snapshot, Rom };
    std::string name;  // as the menu shows it
    Kind kind = Kind::Snapshot;
    Snapshot snapshot;
    std::vector<uint8_t> rom;
    bool operator==(const Game&) const = default;
};

struct Cartridge {
    static constexpr int kSlots = 32;
    static constexpr size_t kSlotSize = 0x4000;
    static constexpr size_t kSize = kSlots * kSlotSize;
    static constexpr int kMaxGames = 31;
    static constexpr int kNameLength = 31;
    static constexpr int kTitleLength = 31;
    // Slot 0 holds the menu, slot 1 the games' headers; the games' memory
    // fills slots 2 to 31, the ROMs taking whole slots from 31 down.
    static constexpr int kFirstDataSlot = 2;

    std::string title = "TuxDAN";
    std::vector<Game> games;
    int autoboot = -1;  // the game that starts by itself, or -1
    bool operator==(const Cartridge&) const = default;
};

// The name as the menu can show it: printable ASCII, at most `length`.
std::string menuText(const std::string& text, int length);

// How much of the cartridge a game takes: a snapshot packed, a ROM its 16K.
size_t gameSize(const Game& game);
// Room for the games: slots 2 to 31.
constexpr size_t kRoom = (Cartridge::kSlots - Cartridge::kFirstDataSlot) * Cartridge::kSlotSize;

// The cartridge's image, 512K, as a .des file holds it. Empty, with the
// reason, if the games do not fit or cannot be put on it.
std::vector<uint8_t> buildCartridge(const Cartridge& cartridge, std::string* error = nullptr);

// A cartridge TuxDAN made, read back. Nothing, with the reason, for one it
// did not make.
std::optional<Cartridge> readCartridge(std::span<const uint8_t> image, std::string* error = nullptr);

}  // namespace tuxdan
