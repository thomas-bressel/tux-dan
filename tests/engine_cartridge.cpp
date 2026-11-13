// Snapshots read and written, and cartridges made of them: what a cartridge
// holds, as its loader reads it, and the cartridge read back as it was.

#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "cartridge.h"
#include "check.h"
#include "pack.h"
#include "snapshot.h"
#include "z80code.h"

using namespace tuxdan;

namespace {

// A snapshot file of version 3: registers that say where they are, memory
// that says which bank it is.
std::vector<uint8_t> snapshotFile(int kb, int machine, uint8_t ramConfig = 0)
{
    std::vector<uint8_t> file(0x100, 0);
    std::memcpy(file.data(), "MV - SNA", 8);
    file[0x10] = 3;
    for (int i = 0; i < 0x1D; ++i)
        file[0x11 + static_cast<size_t>(i)] = static_cast<uint8_t>(0x10 + i);
    file[0x21] = 0xF0;  // SP &BFF0
    file[0x22] = 0xBF;
    file[0x23] = 0x34;  // PC &1234
    file[0x24] = 0x12;
    file[0x25] = 1;     // IM 1
    file[0x1B] = 1;     // IFF1
    file[0x41] = ramConfig;
    file[0x6B] = static_cast<uint8_t>(kb);
    file[0x6C] = static_cast<uint8_t>(kb >> 8);
    file[0x6D] = static_cast<uint8_t>(machine);
    std::mt19937 random(static_cast<unsigned>(kb));
    for (int i = 0; i < kb * 1024; ++i)
        file.push_back(i % 4096 < 3000 ? static_cast<uint8_t>(i >> 14) : static_cast<uint8_t>(random()));
    return file;
}

void testSnapshots()
{
    std::string error;
    const auto small = readSnapshot(snapshotFile(64, 2), &error);
    CHECK(small.has_value());
    if (small) {
        CHECK_EQ(small->ram.size(), 0x10000);
        CHECK_EQ(small->pc(), 0x1234);
        CHECK_EQ(small->sp(), 0xBFF0);
        CHECK_EQ(small->machine(), 2);
        // Written and read again: the same.
        const auto again = readSnapshot(writeSnapshot(*small));
        CHECK(again && *again == *small);
    }
    const auto large = readSnapshot(snapshotFile(128, 2));
    CHECK(large && large->ram.size() == 0x20000 && large->ram[0x1C000] == 7);

    // Memory in a "MEM0" chunk, packed.
    std::vector<uint8_t> chunked = snapshotFile(0, 2);
    const uint8_t chunk[] = {'M', 'E', 'M', '0', 7, 0, 0, 0, 0xE5, 0xFF, 0x42, 0xE5, 0x00, 0x99};
    chunked.insert(chunked.end(), std::begin(chunk), std::end(chunk));
    const auto fromChunk = readSnapshot(chunked, &error);
    CHECK(fromChunk.has_value());
    if (fromChunk) {
        CHECK_EQ(fromChunk->ram.size(), 0x10000);
        CHECK_EQ(fromChunk->ram[0], 0x42);
        CHECK_EQ(fromChunk->ram[254], 0x42);
        CHECK_EQ(fromChunk->ram[255], 0xE5);
        CHECK_EQ(fromChunk->ram[256], 0x99);
    }

    // What a cartridge cannot hold.
    CHECK(!readSnapshot(std::vector<uint8_t>(300, 0), &error) && error.find("not a snapshot") != std::string::npos);
    CHECK(!readSnapshot(snapshotFile(64, 4), &error) && error.find("Plus") != std::string::npos);
    CHECK(!readSnapshot(snapshotFile(256, 2), &error) && error.find("128K") != std::string::npos);
    CHECK(!readSnapshot(snapshotFile(128, 2, 2), &error) && error.find("CP/M") != std::string::npos);
}

void testCartridge()
{
    Cartridge cartridge;
    cartridge.title = "My games";
    Game first;
    first.name = "First game";
    first.snapshot = *readSnapshot(snapshotFile(64, 2));
    Game second;
    second.name = "Second, with \x01 a bad character and a name far too long for the menu";
    second.snapshot = *readSnapshot(snapshotFile(128, 2, 5));
    Game rom;
    rom.name = "A ROM";
    rom.kind = Game::Kind::Rom;
    rom.rom.assign(Cartridge::kSlotSize, 0xC7);
    cartridge.games = {first, second, rom};
    cartridge.autoboot = 1;

    std::string error;
    const auto image = buildCartridge(cartridge, &error);
    CHECK_EQ(image.size(), Cartridge::kSize);
    if (image.size() != Cartridge::kSize)
        return;
    // The menu's code, then the directory.
    CHECK(std::equal(z80::kSlot0.begin(), z80::kSlot0.begin() + z80::DIR, image.begin()));
    CHECK(std::memcmp(&image[z80::DIR], "TXDN", 4) == 0);
    CHECK_EQ(image[z80::DIR_COUNT], 3);
    CHECK_EQ(image[z80::DIR_AUTOBOOT], 1);
    CHECK(std::strcmp(reinterpret_cast<const char*>(&image[z80::DIR_TITLE]), "My games") == 0);
    const auto entry = [&](int g) { return &image[z80::DIR_ENTRIES + static_cast<size_t>(g) * z80::ENTRY_SIZE]; };
    CHECK(std::strcmp(reinterpret_cast<const char*>(entry(0)), "First game") == 0);
    CHECK_EQ(std::strlen(reinterpret_cast<const char*>(entry(1))), Cartridge::kNameLength);
    CHECK(std::strncmp(reinterpret_cast<const char*>(entry(1)), "Second, with ? a bad", 20) == 0);
    CHECK_EQ(entry(2)[z80::ENTRY_TYPE], 1);
    CHECK_EQ(entry(2)[z80::ENTRY_SLOT], 31);  // the ROMs from slot 31 down
    CHECK_EQ(image[31 * Cartridge::kSlotSize], 0xC7);
    CHECK_EQ(image[30 * Cartridge::kSlotSize], 0xFF);

    // A snapshot's table: quarters 1 to 3, the second bank, then &FE with
    // the place below the stack and the game's RAM configuration, then
    // quarter 0 with it, then &FF with the last instructions and the
    // header.
    const uint8_t* header = &image[Cartridge::kSlotSize + z80::HEADER_SIZE];
    const uint8_t* table = header + z80::HEADER_TABLE;
    const uint8_t expected[][2] = {{0xC0, 1}, {0xC0, 2}, {0xC0, 3}, {0xC4, 1}, {0xC5, 1}, {0xC6, 1}, {0xC7, 1}};
    for (int e = 0; e < 7; ++e) {
        CHECK_EQ(table[e * 5], expected[e][0]);
        CHECK_EQ(table[e * 5 + 1], expected[e][1]);
    }
    CHECK_EQ(table[7 * 5 + 1], 0xFE);
    CHECK_EQ(table[7 * 5 + 2] | table[7 * 5 + 3] << 8, 0xBFF0 - 3);
    CHECK_EQ(table[7 * 5 + 4], 0xC5);
    CHECK_EQ(table[8 * 5], 0xC5);
    CHECK_EQ(table[8 * 5 + 1], 0);
    CHECK_EQ(table[9 * 5 + 1], 0xFF);
    const unsigned base = z80::HEADERS + z80::HEADER_SIZE;
    CHECK_EQ(table[9 * 5 + 2] | table[9 * 5 + 3] << 8, base + z80::HEADER_STUB);
    CHECK_EQ(table[9 * 5 + 4] | table[9 * 5 + 5] << 8, base);
    // Each block where its entry says, packed.
    const size_t from = table[2 * 5 + 2] * Cartridge::kSlotSize + (table[2 * 5 + 3] | table[2 * 5 + 4] << 8);
    const auto block = unpack(std::span(image).subspan(from));
    CHECK(block && std::equal(block->begin(), block->end(), second.snapshot.ram.begin() + 3 * kBlockSize));

    // The last instructions: no RET before the last byte, for the
    // cartridge hides itself at the first one fetched.
    const uint8_t* stub = header + z80::HEADER_STUB;
    int length = 0;
    while (stub[length] != 0xC9 && length < 100)
        ++length;
    CHECK(length > 40 && length < 60);
    CHECK_EQ(stub[length - 1], 0xFB);  // IFF1 set: EI just before
    // The registers, in the order they are popped: AF' first, AF last.
    const uint8_t* regs = header + z80::HEADER_REGS;
    CHECK_EQ(regs[0], 0x10 + 21);  // F'
    CHECK_EQ(regs[19], 0x10 + 1);  // A

    // Read back: the same, but the names as the menu has them.
    const auto back = readCartridge(image, &error);
    CHECK(back.has_value());
    if (back) {
        Cartridge expectedBack = cartridge;
        for (Game& game : expectedBack.games)
            game.name = menuText(game.name, Cartridge::kNameLength);
        CHECK(back->title == expectedBack.title && back->autoboot == 1 && back->games.size() == 3);
        for (size_t g = 0; g < back->games.size() && g < 3; ++g)
            CHECK(back->games[g] == expectedBack.games[g]);
    }
    CHECK(!readCartridge(std::vector<uint8_t>(Cartridge::kSize, 0xFF), &error) && error.find("TuxDAN") != std::string::npos);

    // Too much: the menu's length, the cartridge's room.
    Cartridge many;
    many.games.assign(Cartridge::kMaxGames + 1, first);
    CHECK(buildCartridge(many, &error).empty() && error.find("31") != std::string::npos);
    Cartridge full;
    Game noisy = second;
    std::mt19937 random(5);
    for (uint8_t& b : noisy.snapshot.ram)
        b = static_cast<uint8_t>(random());
    full.games.assign(4, noisy);
    CHECK(gameSize(noisy) > 128 * 1024);
    CHECK(buildCartridge(full, &error).empty() && error.find("do not fit") != std::string::npos);
}

}  // namespace

int main()
{
    testSnapshots();
    testCartridge();
    return checkSummary("engine_cartridge");
}
