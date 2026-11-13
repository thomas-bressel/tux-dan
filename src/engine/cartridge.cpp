#include "cartridge.h"

#include <algorithm>
#include <cstring>

#include "pack.h"
#include "z80code.h"

namespace tuxdan {

namespace {

constexpr char kMagic[] = "TXDN";
constexpr uint8_t kVersion = 1;
constexpr uint8_t kLowDone = 0xFE;  // a table's entry: quarter 0 next
constexpr uint8_t kLast = 0xFF;     // and its end

bool fail(std::string* error, const std::string& message)
{
    if (error)
        *error = message;
    return false;
}

// A snapshot's blocks, in the order the loader puts them back: quarters 1
// to 3 of the base RAM, the second bank's four (seen at &4000 with &C4 to
// &C7), quarter 0 last.
struct Block {
    uint8_t config;
    uint8_t target;
    size_t ramOffset;
};

std::vector<Block> blocksOf(const Snapshot& snapshot)
{
    std::vector<Block> blocks;
    for (uint8_t quarter = 1; quarter < 4; ++quarter)
        blocks.push_back({0xC0, quarter, quarter * kBlockSize});
    if (snapshot.ram.size() > Snapshot::kBank)
        for (uint8_t bank = 0; bank < 4; ++bank)
            blocks.push_back({static_cast<uint8_t>(0xC4 + bank), 1, Snapshot::kBank + bank * kBlockSize});
    const uint8_t config = static_cast<uint8_t>(0xC0 | (snapshot.header[Snapshot::kRamConfig] & 7));
    blocks.push_back({config, 0, 0});
    return blocks;
}

// What the loader ends with, for each game: from slot 1 seen at &C000, its
// header at `base`, the cartridge waiting for a RET to hide itself and take
// no more commands; then the registers, popped from the header, and the RET
// to the game's PC, put just below its stack.
std::vector<uint8_t> lastInstructions(const Snapshot& s, uint16_t base)
{
    const uint16_t sp = s.sp();
    const uint16_t pc = s.pc();
    const auto lo = [](unsigned v) { return static_cast<uint8_t>(v); };
    const auto hi = [](unsigned v) { return static_cast<uint8_t>(v >> 8); };
    const uint8_t* r = &s.header[Snapshot::kRegisters];
    const int im = s.header[Snapshot::kIm] & 3;
    // R goes on counting through the four instructions after it is set.
    const uint8_t refresh = static_cast<uint8_t>((r[8] & 0x80) | ((r[8] - 4) & 0x7F));
    const uint16_t regs = static_cast<uint16_t>(base + z80::HEADER_REGS);
    std::vector<uint8_t> code = {
        0x3E, r[9], 0xED, 0x47,                             // ld a,I : ld i,a
        0xED, static_cast<uint8_t>(im == 2 ? 0x5E : im == 1 ? 0x56 : 0x46),
        0xFD, 0x21, lo(sp - 3), hi(sp - 3),                 // ld iy,SP-3
        0x21, lo(pc), hi(pc),                               // ld hl,PC
        0x22, lo(sp - 2), hi(sp - 2),                       // ld (SP-2),hl
        0x3E, 0xE3,                                         // hide both zones at the RET, no more commands
        0xFD, 0xFD, 0xFD, 0x77, 0x00,
        0x31, lo(regs), hi(regs),                           // ld sp,registers
        0xF1, 0x08, 0xC1, 0xD1, 0xE1, 0xD9,                 // AF' BC' DE' HL'
        0xDD, 0xE1, 0xFD, 0xE1,                             // IX IY
        0xC1, 0xD1, 0xE1,                                   // BC DE HL
        0x3E, refresh, 0xED, 0x4F,                          // ld a,R : ld r,a
        0xF1,                                               // AF
        0x31, lo(sp - 2), hi(sp - 2),                       // ld sp,SP-2
    };
    if (r[10] & 1)
        code.push_back(0xFB);  // ei
    code.push_back(0xC9);      // ret: the cartridge goes
    return code;
}

// The registers in the order the last instructions pop them.
std::vector<uint8_t> registersOf(const Snapshot& s)
{
    const uint8_t* r = &s.header[Snapshot::kRegisters];
    // F A C B E D L H R I IFF1 IFF2 IXl IXh IYl IYh SPl SPh PCl PCh IM F' A' C' B' E' D' L' H'
    return {r[21], r[22], r[23], r[24], r[25], r[26], r[27], r[28],  // AF' BC' DE' HL'
            r[12], r[13], r[14], r[15],                              // IX IY
            r[2],  r[3],  r[4],  r[5],  r[6],  r[7],                 // BC DE HL
            r[0],  r[1]};                                            // AF
}

}  // namespace

std::string menuText(const std::string& text, int length)
{
    std::string out;
    for (const char c : text) {
        if (static_cast<int>(out.size()) >= length)
            break;
        out += c >= 32 && c < 127 ? c : '?';
    }
    return out;
}

size_t gameSize(const Game& game)
{
    if (game.kind == Game::Kind::Rom)
        return Cartridge::kSlotSize;
    size_t size = 0;
    for (const Block& block : blocksOf(game.snapshot))
        size += pack(std::span(game.snapshot.ram).subspan(block.ramOffset, kBlockSize)).size();
    return size;
}

std::vector<uint8_t> buildCartridge(const Cartridge& cartridge, std::string* error)
{
    const auto games = cartridge.games;
    if (games.size() > static_cast<size_t>(Cartridge::kMaxGames)) {
        fail(error, "a cartridge has room in its menu for " + std::to_string(Cartridge::kMaxGames) + " games");
        return {};
    }
    std::vector<uint8_t> image(Cartridge::kSize, 0xFF);
    std::copy(z80::kSlot0.begin(), z80::kSlot0.end(), image.begin());

    // The directory.
    uint8_t* dir = &image[z80::DIR];
    std::fill_n(dir, z80::FONT - z80::DIR, uint8_t{0});
    std::memcpy(dir, kMagic, 4);
    dir[4] = kVersion;
    dir[5] = static_cast<uint8_t>(games.size());
    dir[6] = cartridge.autoboot >= 0 && cartridge.autoboot < static_cast<int>(games.size())
                 ? static_cast<uint8_t>(cartridge.autoboot)
                 : 0xFF;
    const std::string title = menuText(cartridge.title, Cartridge::kTitleLength);
    std::memcpy(&image[z80::DIR_TITLE], title.data(), title.size());

    size_t data = Cartridge::kFirstDataSlot * Cartridge::kSlotSize;  // where the next block goes
    int romSlot = Cartridge::kSlots - 1;                             // and the next ROM
    for (size_t g = 0; g < games.size(); ++g) {
        const Game& game = games[g];
        uint8_t* entry = &image[z80::DIR_ENTRIES + g * z80::ENTRY_SIZE];
        const std::string name = menuText(game.name, Cartridge::kNameLength);
        std::memcpy(entry, name.data(), name.size());
        if (game.kind == Game::Kind::Rom) {
            if (game.rom.empty() || game.rom.size() > Cartridge::kSlotSize) {
                fail(error, "\"" + game.name + "\" is not a ROM of 16K");
                return {};
            }
            if (static_cast<size_t>(romSlot) * Cartridge::kSlotSize < data) {
                fail(error, "the games do not fit on the cartridge");
                return {};
            }
            entry[z80::ENTRY_TYPE] = 1;
            entry[z80::ENTRY_SLOT] = static_cast<uint8_t>(romSlot);
            std::copy(game.rom.begin(), game.rom.end(), image.begin() + static_cast<std::ptrdiff_t>(romSlot * Cartridge::kSlotSize));
            --romSlot;
            continue;
        }
        const Snapshot& s = game.snapshot;
        if (s.ram.size() != Snapshot::kBank && s.ram.size() != 2 * Snapshot::kBank) {
            fail(error, "\"" + game.name + "\" has neither 64K nor 128K of memory");
            return {};
        }
        entry[z80::ENTRY_TYPE] = 0;
        // Its header, in slot 1.
        const size_t headerAt = Cartridge::kSlotSize + g * z80::HEADER_SIZE;
        const auto base = static_cast<uint16_t>(z80::HEADERS + g * z80::HEADER_SIZE);
        uint8_t* header = &image[headerAt];
        std::fill_n(header, z80::HEADER_SIZE, uint8_t{0});
        std::copy(s.header.begin(), s.header.end(), header);
        uint8_t* table = header + z80::HEADER_TABLE;
        for (const Block& block : blocksOf(s)) {
            if (block.target == 0) {
                // Quarter 0 next: commands write below the game's stack.
                const uint16_t spoil = static_cast<uint16_t>(s.sp() - 3);
                *table++ = 0;
                *table++ = kLowDone;
                *table++ = static_cast<uint8_t>(spoil);
                *table++ = static_cast<uint8_t>(spoil >> 8);
                *table++ = block.config;
            }
            const auto packed = pack(std::span(s.ram).subspan(block.ramOffset, kBlockSize));
            const size_t room = static_cast<size_t>(romSlot + 1) * Cartridge::kSlotSize;
            if (data + packed.size() > room) {
                fail(error, "the games do not fit on the cartridge");
                return {};
            }
            *table++ = block.config;
            *table++ = block.target;
            *table++ = static_cast<uint8_t>(data / Cartridge::kSlotSize);
            *table++ = static_cast<uint8_t>(data % Cartridge::kSlotSize);
            *table++ = static_cast<uint8_t>(data % Cartridge::kSlotSize >> 8);
            std::copy(packed.begin(), packed.end(), image.begin() + static_cast<std::ptrdiff_t>(data));
            data += packed.size();
        }
        const auto stub = static_cast<uint16_t>(base + z80::HEADER_STUB);
        *table++ = 0;
        *table++ = kLast;
        *table++ = static_cast<uint8_t>(stub);
        *table++ = static_cast<uint8_t>(stub >> 8);
        *table++ = static_cast<uint8_t>(base);
        *table++ = static_cast<uint8_t>(base >> 8);
        const auto regs = registersOf(s);
        std::copy(regs.begin(), regs.end(), header + z80::HEADER_REGS);
        const auto last = lastInstructions(s, base);
        std::copy(last.begin(), last.end(), header + z80::HEADER_STUB);
    }
    return image;
}

std::optional<Cartridge> readCartridge(std::span<const uint8_t> image, std::string* error)
{
    if (image.size() < 2 * Cartridge::kSlotSize || std::memcmp(&image[z80::DIR], kMagic, 4) != 0) {
        fail(error, "this cartridge was not made by TuxDAN");
        return std::nullopt;
    }
    if (image[z80::DIR + 4] != kVersion) {
        fail(error, "this cartridge was made by another version of TuxDAN");
        return std::nullopt;
    }
    Cartridge cartridge;
    const int count = std::min<int>(image[z80::DIR_COUNT], Cartridge::kMaxGames);
    cartridge.autoboot = image[z80::DIR_AUTOBOOT] < count ? image[z80::DIR_AUTOBOOT] : -1;
    const auto text = [&](size_t at, int length) {
        std::string out;
        for (int i = 0; i < length && image[at + static_cast<size_t>(i)]; ++i)
            out += static_cast<char>(image[at + static_cast<size_t>(i)]);
        return out;
    };
    cartridge.title = text(z80::DIR_TITLE, Cartridge::kTitleLength);
    std::vector<uint8_t> whole(image.begin(), image.end());
    whole.resize(Cartridge::kSize, 0xFF);
    for (int g = 0; g < count; ++g) {
        const size_t at = z80::DIR_ENTRIES + static_cast<size_t>(g) * z80::ENTRY_SIZE;
        Game game;
        game.name = text(at, Cartridge::kNameLength);
        if (image[at + z80::ENTRY_TYPE] == 1) {
            game.kind = Game::Kind::Rom;
            const size_t slot = image[at + z80::ENTRY_SLOT] % Cartridge::kSlots;
            game.rom.assign(whole.begin() + static_cast<std::ptrdiff_t>(slot * Cartridge::kSlotSize),
                            whole.begin() + static_cast<std::ptrdiff_t>((slot + 1) * Cartridge::kSlotSize));
        } else {
            const size_t headerAt = Cartridge::kSlotSize + static_cast<size_t>(g) * z80::HEADER_SIZE;
            std::copy_n(whole.begin() + static_cast<std::ptrdiff_t>(headerAt), Snapshot::kHeaderSize, game.snapshot.header.begin());
            const size_t kb = game.snapshot.word(Snapshot::kDumpSize);
            game.snapshot.ram.assign(kb > 64 ? 2 * Snapshot::kBank : Snapshot::kBank, 0);
            for (size_t e = headerAt + z80::HEADER_TABLE; e + 5 <= headerAt + z80::HEADER_REGS; e += 5) {
                if (whole[e + 1] == kLast)
                    break;
                if (whole[e + 1] == kLowDone)
                    continue;
                const uint8_t config = whole[e];
                const uint8_t target = whole[e + 1] & 3;
                const size_t from = whole[e + 2] * Cartridge::kSlotSize + (whole[e + 3] | whole[e + 4] << 8);
                const auto block = from < whole.size() ? unpack(std::span(whole).subspan(from)) : std::nullopt;
                if (!block || block->size() != kBlockSize) {
                    fail(error, "the memory of \"" + game.name + "\" is damaged");
                    return std::nullopt;
                }
                // The second bank's blocks are seen at &4000; quarter 0 has
                // the game's own configuration, which leaves it in place.
                const size_t ram = target == 1 && config >= 0xC4 ? Snapshot::kBank + (config - 0xC4u) * kBlockSize
                                                                 : target * kBlockSize;
                if (ram + kBlockSize <= game.snapshot.ram.size())
                    std::copy(block->begin(), block->end(), game.snapshot.ram.begin() + static_cast<std::ptrdiff_t>(ram));
            }
        }
        cartridge.games.push_back(std::move(game));
    }
    return cartridge;
}

}  // namespace tuxdan
