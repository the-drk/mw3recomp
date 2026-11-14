// Real no-SDL input imports against synthetic guest memory, never game assets.
#include "tests/fake_guest.h"
#include "runtime/console.h"
#include "runtime/online/service.h"
#include <cstdlib>
#include <cstring>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

PPC_FUNC(__imp__XamInputGetState);
PPC_FUNC(__imp__XamInputGetCapabilities);
PPC_FUNC(__imp__XamInputSetState);
namespace online { void Frame() {} }
namespace console { void Pump(PPCContext&, unsigned char*) {} }

int main(int argc, char** argv)
{
    // Separate CTest processes isolate the once-only script configuration.
    const bool scripted = argc > 1 && std::strcmp(argv[1], "scripted") == 0;
    if (scripted)
        setenv("MW2_INPUT_SCRIPT", "0:start:3600,0:lt:3600,0:lx-:3600,0:a@2:3600", 1);
    else
        unsetenv("MW2_INPUT_SCRIPT");
    PPCContext ctx{};
    auto* base = guest::Base();
    constexpr uint32_t address = 0x1000;
    ctx.r3.u64 = 0; ctx.r5.u64 = address;
    __imp__XamInputGetCapabilities(ctx, base);
    assert(ctx.r3.u32 == (scripted ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED));
    if (scripted) assert(base[address] == 1 && base[address + 1] == 1);
    ctx.r3.u64 = 0;
    __imp__XamInputGetState(ctx, base);
    assert(ctx.r3.u32 == (scripted ? X_ERROR_SUCCESS : X_ERROR_DEVICE_NOT_CONNECTED));
    if (scripted)
    {
        assert(uint16_t(*GuestPtr<be16>(address + 4)) == 0x0010);
        assert(base[address + 6] == 255);
        assert(uint16_t(*GuestPtr<be16>(address + 8)) == 0x8000);
        const uint32_t packet = *GuestPtr<be32>(address);
        ctx.r3.u64 = 0;
        __imp__XamInputGetState(ctx, base);
        assert(uint32_t(*GuestPtr<be32>(address)) == packet);
        ctx.r3.u64 = 1;
        __imp__XamInputGetState(ctx, base);
        assert(ctx.r3.u32 == X_ERROR_SUCCESS);
        assert(uint16_t(*GuestPtr<be16>(address + 4)) == 0x1000);
    }
    for (uint32_t user : {2u, 4u, 0xFFFFFFFFu})
    {
        ctx.r3.u64 = user;
        __imp__XamInputGetState(ctx, base);
        assert(ctx.r3.u32 == X_ERROR_DEVICE_NOT_CONNECTED);
        assert(uint32_t(*GuestPtr<be32>(address)) == 0);
    }
}
