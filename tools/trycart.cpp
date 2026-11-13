// Tries TuxDAN's cartridges in TuxAPE's own machine: for each snapshot
// given, a cartridge is made with all of them, started on a CPC with a
// Dandanator, its menu pictured, and each game played in turn (it starts by
// itself). When the cartridge hides itself, the machine's memory and Z80
// are compared with the snapshot's, then a picture is taken after a few
// seconds of the game.
//
//   g++ -std=c++20 -O1 -I../TuxAPE/src -Isrc/engine tools/trycart.cpp src/engine/*.cpp \
//       ../TuxAPE/build/src/core/libtuxape_core.a -o build/trycart
//   build/trycart OUT_FOLDER game.sna...
//
// A developer's tool: the plugin needs no part of TuxAPE.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "cartridge.h"
#include "core/cpc.h"
#include "core/files.h"
#include "core/setup.h"
#include "snapshot.h"

using namespace tuxape;

namespace {

void picture(Cpc& cpc, const std::string& path)
{
    const uint32_t* px = cpc.monitor().frame();
    const int w = cpc.monitor().width(), h = cpc.monitor().height();
    FILE* out = std::fopen(path.c_str(), "wb");
    std::fprintf(out, "P6 %d %d 255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        const uint8_t rgb[3] = {uint8_t(px[i] >> 16), uint8_t(px[i] >> 8), uint8_t(px[i])};
        std::fwrite(rgb, 1, 3, out);
    }
    std::fclose(out);
}

bool start(Cpc& cpc, const std::string& des, int machine)
{
    MachineConfig config = stockMachine(machine == 0 ? CpcModel::Cpc464 : machine == 1 ? CpcModel::Cpc664 : CpcModel::Cpc6128);
    config.cards[0] = Card::Dandanator;
    config.dandanatorFile = des;
    std::string error;
    if (!applyMachine(cpc, config, defaultRomDir(), &error)) {
        std::printf("cannot set the machine up: %s\n", error.c_str());
        return false;
    }
    cpc.coldReset();
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: trycart OUT_FOLDER game.sna...\n");
        return 2;
    }
    const std::string out = argv[1];
    tuxdan::Cartridge cartridge;
    cartridge.title = "TuxDAN test cartridge";
    for (int a = 2; a < argc; ++a) {
        const auto file = readFile(argv[a]);
        std::string error;
        const auto snapshot = file ? tuxdan::readSnapshot(*file, &error) : std::nullopt;
        if (!snapshot) {
            std::printf("%s: %s\n", argv[a], error.c_str());
            continue;
        }
        tuxdan::Game game;
        std::string name = argv[a];
        name = name.substr(name.find_last_of('/') + 1);
        game.name = name.substr(0, name.find_last_of('.'));
        game.snapshot = *snapshot;
        std::printf("%-60.60s %3zuK packed to %6zu bytes, machine %d\n", game.name.c_str(), snapshot->ram.size() / 1024,
                    tuxdan::gameSize(game), snapshot->machine());
        cartridge.games.push_back(game);
    }
    std::string error;
    auto image = tuxdan::buildCartridge(cartridge, &error);
    if (image.empty()) {
        std::printf("no cartridge: %s\n", error.c_str());
        return 1;
    }
    const auto back = tuxdan::readCartridge(image, &error);
    std::printf("read back: %s\n", back && *back == cartridge ? "the same" : ("different " + error).c_str());

    // The menu.
    const std::string des = out + "/test.des";
    writeFile(des, image);
    {
        Cpc cpc;
        if (!start(cpc, des, 2))
            return 1;
        for (int f = 0; f < 50; ++f)
            cpc.runFrame();
        picture(cpc, out + "/menu.ppm");
    }

    int failures = 0;
    for (size_t g = 0; g < cartridge.games.size(); ++g) {
        const tuxdan::Snapshot& s = cartridge.games[g].snapshot;
        tuxdan::Cartridge one = cartridge;
        one.autoboot = static_cast<int>(g);
        writeFile(des, tuxdan::buildCartridge(one));
        Cpc cpc;
        if (!start(cpc, des, s.machine()))
            return 1;
        long steps = 0;
        const bool trace = std::getenv("TRYCART_TRACE") != nullptr;
        auto before = cpc.dandanator()->state();
        while (!cpc.dandanator()->state().commandsOff && steps < 20'000'000) {
            const uint16_t pc = cpc.cpu().pc;
            cpc.stepInstruction();
            ++steps;
            const auto now = cpc.dandanator()->state();
            if (trace && std::memcmp(&now, &before, sizeof now) != 0)
                std::printf("  %04X z0 %s%2d@%d z1 %s%2d@%d wait %d\n", pc, now.shown[0] ? "on " : "off", now.slot[0], now.high[0],
                            now.shown[1] ? "on " : "off", now.slot[1], now.high[1], now.waitingRet);
            before = now;
        }
        const auto& z = cpc.cpu();
        const uint8_t* r = &s.header[tuxdan::Snapshot::kRegisters];
        const auto pair = [&](int high, int low) { return z.reg[high] << 8 | z.reg[low]; };
        const auto word = [&](int at) { return r[at] | r[at + 1] << 8; };
        std::vector<std::string> wrong;
        const auto check = [&](const char* what, int got, int want) {
            if (got != want) {
                char text[80];
                std::snprintf(text, sizeof text, "%s %04X want %04X", what, got, want);
                wrong.push_back(text);
            }
        };
        check("PC", z.pc, s.pc());
        check("SP", z.sp, s.sp());
        check("AF", pair(z.A, z.F), r[1] << 8 | r[0]);
        check("BC", pair(z.B, z.C), r[3] << 8 | r[2]);
        check("DE", pair(z.D, z.E), r[5] << 8 | r[4]);
        check("HL", pair(z.H, z.L), r[7] << 8 | r[6]);
        check("IX", z.ix, word(12));
        check("IY", z.iy, word(14));
        check("AF'", z.af2, r[22] << 8 | r[21]);
        check("BC'", z.bc2, r[24] << 8 | r[23]);
        check("DE'", z.de2, r[26] << 8 | r[25]);
        check("HL'", z.hl2, r[28] << 8 | r[27]);
        check("I", z.i, r[9]);
        check("IM", z.im, s.header[tuxdan::Snapshot::kIm] & 3);
        check("IFF1", z.iff1, r[10] & 1);
        int ramDiff = 0, first = -1;
        for (size_t page = 0; page * tuxdan::Snapshot::kBank < s.ram.size(); ++page) {
            const uint8_t* ram = cpc.memory().ramPage(static_cast<int>(page));
            for (size_t a = 0; ram && a < tuxdan::Snapshot::kBank; ++a) {
                const size_t at = page * tuxdan::Snapshot::kBank + a;
                // Below the stack: spoiled by the loader, as by an interrupt.
                const bool spoilt = page == 0 && ((a - s.sp() + 3) & 0xFFFF) < 3;
                if (!spoilt && ram[a] != s.ram[at]) {
                    if (ramDiff < 12 && std::getenv("TRYCART_DETAIL"))
                        std::printf("    %06zX got %02X want %02X\n", at, ram[a], s.ram[at]);
                    if (first < 0)
                        first = static_cast<int>(at);
                    ++ramDiff;
                }
            }
        }
        if (ramDiff)
            wrong.push_back(std::to_string(ramDiff) + " bytes of RAM differ, the first at " + std::to_string(first));
        const auto state = cpc.dandanator()->state();
        if (state.shown[0] || state.shown[1])
            wrong.push_back("the cartridge is still shown");
        std::printf("%-50.50s %8ld instructions: %s\n", cartridge.games[g].name.c_str(), steps, wrong.empty() ? "as in the snapshot" : "");
        for (const std::string& w : wrong)
            std::printf("    %s\n", w.c_str());
        failures += !wrong.empty();
        for (int f = 0; f < 150; ++f)
            cpc.runFrame();
        picture(cpc, out + "/game" + std::to_string(g) + ".ppm");
    }
    return failures ? 1 : 0;
}
