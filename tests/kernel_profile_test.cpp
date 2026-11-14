// Asset-free profile ABI test: synthetic settings, guest memory and save root.
#include "tests/fake_guest.h"
#include "runtime/signin.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <filesystem>

PPC_FUNC(__imp__XamUserReadProfileSettings);
PPC_FUNC(__imp__XamUserWriteProfileSettings);

namespace kernel
{
    const std::filesystem::path& SaveRoot()
    {
        static const auto path = std::filesystem::temp_directory_path() /
            ("mw3-profile-test-" + std::to_string(SystemTime100ns()));
        return path;
    }
    void CompleteOverlapped(uint32_t address, uint32_t result)
    {
        assert(address == 0 && result == X_ERROR_SUCCESS);
    }
}
namespace signin { Player At(uint32_t) { return {}; } }

int main()
{
    auto* base = guest::Base();
    PPCContext ctx{};
    ctx.r1.u64 = 0x8000; // Ninth read argument (overlapped) is zero in fake memory.
    constexpr uint32_t settings = 0x1000, ids = 0x2000, size = 0x2100;
    constexpr uint32_t output = 0x3000, payload = 0x4000;
    constexpr uint32_t binaryId = 0x63E83FFF, integerId = 0x10040002;
    const uint8_t bytes[] = {0x00, 0xA5, 0xFF, 0x37};
    std::memcpy(base + payload, bytes, sizeof bytes);
    auto put = [&](uint32_t address, uint32_t value) { *GuestPtr<be32>(address) = value; };
    put(settings, 2); put(settings + 16, binaryId);
    base[settings + 24] = 6;
    put(settings + 32, sizeof bytes); put(settings + 36, payload);
    put(settings + 40, 1); put(settings + 56, integerId);
    base[settings + 64] = 1;
    put(settings + 72, 0x12345678);
    ctx.r3.u64 = 0; ctx.r4.u64 = 0; ctx.r5.u64 = 2;
    ctx.r6.u64 = settings; ctx.r7.u64 = 0;
    __imp__XamUserWriteProfileSettings(ctx, base);
    assert(ctx.r3.u32 == X_ERROR_SUCCESS);
    assert(std::filesystem::exists(kernel::SaveRoot() / "profile.bin"));

    put(ids, binaryId); put(ids + 4, integerId); put(ids + 8, 0x10040003);
    auto read = [&](uint32_t buffer) {
        ctx.r3.u64 = 0; ctx.r4.u64 = 0; ctx.r5.u64 = 0; ctx.r6.u64 = 0;
        ctx.r7.u64 = 3; ctx.r8.u64 = ids; ctx.r9.u64 = size; ctx.r10.u64 = buffer;
        __imp__XamUserReadProfileSettings(ctx, base);
    };
    read(0);
    assert(ctx.r3.u32 == 122);
    assert(uint32_t(*GuestPtr<be32>(size)) == 8 + 3 * 40 + sizeof bytes);
    std::memset(base + output, 0xCC, uint32_t(*GuestPtr<be32>(size)));
    read(output);
    assert(ctx.r3.u32 == X_ERROR_SUCCESS);
    assert(uint32_t(*GuestPtr<be32>(output)) == 3);
    assert(uint32_t(*GuestPtr<be32>(output + 4)) == output + 8);
    const uint32_t first = output + 8, second = first + 40, missing = second + 40;
    assert(base[first + 24] == 6); // Type is a byte, not a big-endian word.
    assert(base[second + 24] == 1);
    for (uint32_t offset = 25; offset < 32; ++offset) assert(base[first + offset] == 0);
    assert(uint32_t(*GuestPtr<be32>(first)) == 2);
    assert(uint32_t(*GuestPtr<be32>(first + 32)) == sizeof bytes);
    const uint32_t data = *GuestPtr<be32>(first + 36);
    assert(data == output + 8 + 3 * 40);
    assert(std::memcmp(base + data, bytes, sizeof bytes) == 0);
    assert(uint32_t(*GuestPtr<be32>(second + 32)) == 0x12345678);
    assert(uint32_t(*GuestPtr<be32>(missing)) == 0);
    assert(base[missing + 24] == 0);
    // Delete only this test's newly-created file and now-empty private directory.
    std::filesystem::remove(kernel::SaveRoot() / "profile.bin");
    std::filesystem::remove(kernel::SaveRoot());
}
