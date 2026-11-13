#include "snapshot.h"

#include <algorithm>
#include <cstring>

namespace tuxdan {

namespace {

constexpr char kSignature[] = "MV - SNA";

bool fail(std::string* error, const char* message)
{
    if (error)
        *error = message;
    return false;
}

// A "MEMn" chunk: run-length encoded (&E5 count value, &E5 &00 for one
// &E5), unless it is 64K long.
bool unpackChunk(std::span<const uint8_t> in, uint8_t* out)
{
    if (in.size() == Snapshot::kBank) {
        std::memcpy(out, in.data(), Snapshot::kBank);
        return true;
    }
    size_t o = 0;
    for (size_t i = 0; i < in.size() && o < Snapshot::kBank;) {
        if (in[i] == 0xE5 && i + 1 < in.size()) {
            const int count = in[i + 1];
            if (count == 0) {
                out[o++] = 0xE5;
                i += 2;
            } else if (i + 2 < in.size()) {
                const size_t run = std::min<size_t>(static_cast<size_t>(count), Snapshot::kBank - o);
                std::memset(out + o, in[i + 2], run);
                o += run;
                i += 3;
            } else {
                return false;
            }
        } else {
            out[o++] = in[i++];
        }
    }
    return true;
}

}  // namespace

std::optional<Snapshot> readSnapshot(std::span<const uint8_t> file, std::string* error)
{
    if (file.size() < Snapshot::kHeaderSize || std::memcmp(file.data(), kSignature, 8) != 0) {
        fail(error, "this is not a snapshot file");
        return std::nullopt;
    }
    Snapshot snapshot;
    std::copy_n(file.begin(), Snapshot::kHeaderSize, snapshot.header.begin());
    const int version = file[Snapshot::kVersion];
    if (version < 1 || version > 3) {
        fail(error, "this snapshot is of a version that is not known");
        return std::nullopt;
    }
    if (version >= 2 && file[Snapshot::kMachine] >= 3 && file[Snapshot::kMachine] <= 6) {
        fail(error, "this is a Plus's snapshot: the Dandanator's loader cannot put back its ASIC");
        return std::nullopt;
    }
    std::array<std::vector<uint8_t>, 2> banks;
    int bankCount = 0;
    const auto bank = [&](int n) -> uint8_t* {
        if (banks[n].empty())
            banks[n].assign(Snapshot::kBank, 0);
        bankCount = std::max(bankCount, n + 1);
        return banks[n].data();
    };
    const size_t dump = static_cast<size_t>(snapshot.word(Snapshot::kDumpSize)) * 1024;
    if (dump > 2 * Snapshot::kBank) {
        fail(error, "this snapshot has more than 128K of memory");
        return std::nullopt;
    }
    for (int n = 0; static_cast<size_t>(n) * Snapshot::kBank < dump; ++n) {
        const size_t from = Snapshot::kHeaderSize + static_cast<size_t>(n) * Snapshot::kBank;
        uint8_t* to = bank(n);
        if (from < file.size())
            std::memcpy(to, &file[from], std::min(Snapshot::kBank, file.size() - from));
    }
    size_t pos = Snapshot::kHeaderSize + dump;
    while (version >= 3 && pos + 8 <= file.size()) {
        const size_t length = file[pos + 4] | file[pos + 5] << 8 | file[pos + 6] << 16
                              | static_cast<size_t>(file[pos + 7]) << 24;
        const auto body = file.subspan(pos + 8, std::min(length, file.size() - pos - 8));
        if (std::memcmp(&file[pos], "MEM", 3) == 0 && file[pos + 3] >= '0' && file[pos + 3] <= '8') {
            const int n = file[pos + 3] - '0';
            if (n >= 2) {
                fail(error, "this snapshot has more than 128K of memory");
                return std::nullopt;
            }
            if (!unpackChunk(body, bank(n))) {
                fail(error, "this snapshot's memory is damaged");
                return std::nullopt;
            }
        } else if (std::memcmp(&file[pos], "CPC+", 4) == 0) {
            fail(error, "this is a Plus's snapshot: the Dandanator's loader cannot put back its ASIC");
            return std::nullopt;
        }
        pos += 8 + length;
    }
    if (bankCount == 0) {
        fail(error, "this snapshot holds no memory");
        return std::nullopt;
    }
    // The configurations that move the base RAM (CP/M's) cannot be put back
    // by the loader, which unpacks quarter 0 last with the game's own.
    const int config = snapshot.header[Snapshot::kRamConfig] & 7;
    if (config >= 1 && config <= 3) {
        fail(error, "this snapshot has the RAM in one of CP/M's configurations, which the loader cannot put back");
        return std::nullopt;
    }
    for (int n = 0; n < bankCount; ++n) {
        if (banks[n].empty())
            banks[n].assign(Snapshot::kBank, 0);
        snapshot.ram.insert(snapshot.ram.end(), banks[n].begin(), banks[n].end());
    }
    snapshot.header[Snapshot::kDumpSize] = static_cast<uint8_t>(snapshot.ram.size() / 1024);
    snapshot.header[Snapshot::kDumpSize + 1] = static_cast<uint8_t>(snapshot.ram.size() / 1024 >> 8);
    return snapshot;
}

std::vector<uint8_t> writeSnapshot(const Snapshot& snapshot)
{
    std::vector<uint8_t> file(snapshot.header.begin(), snapshot.header.end());
    std::memcpy(file.data(), kSignature, 8);
    if (file[Snapshot::kVersion] < 3)
        file[Snapshot::kVersion] = 3;
    file[Snapshot::kDumpSize] = static_cast<uint8_t>(snapshot.ram.size() / 1024);
    file[Snapshot::kDumpSize + 1] = static_cast<uint8_t>(snapshot.ram.size() / 1024 >> 8);
    file.insert(file.end(), snapshot.ram.begin(), snapshot.ram.end());
    return file;
}

}  // namespace tuxdan
