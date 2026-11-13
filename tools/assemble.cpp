// Assembles z80/cartridge.asm with TuxAPE's own assembler and writes what it
// gives as src/engine/z80code.h: slot 0 of a TuxDAN cartridge, its menu and
// its loader, with the directory and the games left for the plugin to fill.
// Run by hand when the Z80 code changes (the plugin itself needs no
// assembler, nor any part of TuxAPE):
//
//   g++ -std=c++20 -O1 -I../TuxAPE/src tools/assemble.cpp ../TuxAPE/src/core/assembler.cpp -o build/assemble
//   build/assemble z80/cartridge.asm src/engine/z80code.h
//
// Code assembled at &0000-&3FFF lands at the same place in the slot; code
// assembled at &8000-&BFFF (the loader's copy that runs with zone 0 at
// &8000) lands &8000 lower.

#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/assembler.h"

namespace {

std::string folderOf(const std::string& path)
{
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::optional<std::string> readText(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
        return std::nullopt;
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: assemble cartridge.asm z80code.h\n");
        return 2;
    }
    const std::string source = argv[1];
    const auto text = readText(source);
    if (!text) {
        std::fprintf(stderr, "cannot read %s\n", source.c_str());
        return 1;
    }
    tuxape::AsmHost host;
    host.source = [&](std::string& name, const std::string&) { name = folderOf(source) + name; return readText(name); };
    const tuxape::AsmResult result = tuxape::assemble(*text, source, host);
    for (const tuxape::AsmError& error : result.errors)
        std::fprintf(stderr, "%s:%d: %s\n", error.file.c_str(), error.line, error.message.c_str());
    if (!result.ok())
        return 1;

    std::vector<int> slot(0x4000, -1);
    for (const tuxape::AsmBlock& block : result.memory) {
        for (size_t n = 0; n < block.data.size(); ++n) {
            const int address = block.address + static_cast<int>(n);
            const int offset = address < 0x4000 ? address : address >= 0x8000 && address < 0xC000 ? address - 0x8000 : -1;
            if (offset < 0) {
                std::fprintf(stderr, "code at &%04X is in no place of the slot\n", address);
                return 1;
            }
            if (slot[static_cast<size_t>(offset)] >= 0) {
                std::fprintf(stderr, "two pieces of code at offset &%04X\n", offset);
                return 1;
            }
            slot[static_cast<size_t>(offset)] = block.data[n];
        }
    }
    int last = 0;
    for (int n = 0; n < 0x4000; ++n)
        if (slot[static_cast<size_t>(n)] >= 0)
            last = n + 1;

    std::ofstream out(argv[2]);
    out << "// Made by tools/assemble.cpp from z80/cartridge.asm: do not edit.\n#pragma once\n\n"
           "#include <array>\n#include <cstdint>\n\nnamespace tuxdan::z80 {\n\n";
    // The symbols the plugin needs: those whose names are in capitals.
    for (const tuxape::AsmSymbol& symbol : result.symbols) {
        bool capitals = !symbol.name.empty();
        for (char c : symbol.name)
            capitals = capitals && (std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_');
        if (capitals)
            out << "constexpr int " << symbol.name << " = 0x" << std::hex << symbol.value << std::dec << ";\n";
    }
    out << "\n// Slot 0 up to its last byte of code; what is not code is &FF.\ninline constexpr std::array<uint8_t, " << last
        << "> kSlot0 = {\n";
    for (int n = 0; n < last; ++n) {
        char hex[16];
        std::snprintf(hex, sizeof hex, "0x%02X,", slot[static_cast<size_t>(n)] < 0 ? 0xFF : slot[static_cast<size_t>(n)]);
        out << (n % 16 == 0 ? "    " : "") << hex << (n % 16 == 15 ? "\n" : "");
    }
    out << "\n};\n\n}  // namespace tuxdan::z80\n";
    std::printf("%d bytes of slot 0, %d of code\n", last, result.bytes);
    return 0;
}
