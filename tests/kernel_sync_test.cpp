// Exercise real synchronization imports using synthetic guest memory only.
#include "tests/fake_guest.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <algorithm>
#include <thread>
#include <cstdio>

PPC_FUNC(__imp__RtlInitializeCriticalSectionAndSpinCount);
PPC_FUNC(__imp__RtlEnterCriticalSection);
PPC_FUNC(__imp__RtlLeaveCriticalSection);
PPC_FUNC(__imp__RtlTryEnterCriticalSection);
PPC_FUNC(__imp__KfAcquireSpinLock);
PPC_FUNC(__imp__KfReleaseSpinLock);
PPC_FUNC(__imp__KeTryToAcquireSpinLockAtRaisedIrql);

using namespace kernel;

int main()
{
    constexpr uint32_t cs = 0x1000, lock = 0x2000;
    PPCContext a{}, b{};
    a.r13.u32 = 0x3000;
    b.r13.u32 = 0x4000;
    *GuestPtr<be32>(a.r13.u32 + 256) = 0x5000;
    *GuestPtr<be32>(b.r13.u32 + 256) = 0x6000;
    // Spin count is rounded up in units of 256 and saturated, never wrapped.
    for (uint32_t spins : {0u, 1u, 256u, 257u, 65280u, 0xFFFFFFFFu})
    {
        a.r3.u32 = cs;
        a.r4.u32 = spins;
        __imp__RtlInitializeCriticalSectionAndSpinCount(a, guest::Base());
        assert(a.r3.u32 == X_STATUS_SUCCESS);
        const uint32_t expected = uint32_t(std::min<uint64_t>((uint64_t(spins) + 255) / 256, 255));
        assert(*GuestPtr<uint8_t>(cs + 1) == expected);
        assert(*GuestPtr<be32>(cs + 16) == 0xFFFFFFFFu);
        assert(*GuestPtr<be32>(cs + 20) == 0);
        assert(*GuestPtr<be32>(cs + 24) == 0);
    }
    a.r3.u32 = cs;
    __imp__RtlEnterCriticalSection(a, guest::Base());
    a.r3.u32 = cs;
    __imp__RtlTryEnterCriticalSection(a, guest::Base());
    assert(a.r3.u32 == 1);
    assert(*GuestPtr<be32>(cs + 20) == 2);
    b.r3.u32 = cs;
    __imp__RtlTryEnterCriticalSection(b, guest::Base());
    assert(b.r3.u32 == 0);
    a.r3.u32 = cs;
    __imp__RtlLeaveCriticalSection(a, guest::Base());
    assert(*GuestPtr<be32>(cs + 20) == 1);
    a.r3.u32 = cs;
    __imp__RtlLeaveCriticalSection(a, guest::Base());
    assert(*GuestPtr<be32>(cs + 16) == 0xFFFFFFFFu);
    assert(*GuestPtr<be32>(cs + 24) == 0);

    a.r3.u32 = lock;
    __imp__KfAcquireSpinLock(a, guest::Base());
    assert(*GuestPtr<be32>(lock) == a.r13.u32);
    b.r3.u32 = lock;
    __imp__KeTryToAcquireSpinLockAtRaisedIrql(b, guest::Base());
    assert(b.r3.u32 == 0);
    a.r3.u32 = lock;
    __imp__KfReleaseSpinLock(a, guest::Base());
    assert(*GuestPtr<be32>(lock) == 0);
    // Two host threads must serialize updates to an ordinary protected value.
    uint32_t count = 0;
    auto worker = [&](PPCContext ctx) {
        for (unsigned i = 0; i < 10000; ++i)
        {
            ctx.r3.u32 = lock;
            __imp__KfAcquireSpinLock(ctx, guest::Base());
            ++count;
            ctx.r3.u32 = lock;
            __imp__KfReleaseSpinLock(ctx, guest::Base());
        }
    };
    std::thread first(worker, a), second(worker, b);
    first.join();
    second.join();
    assert(count == 20000);
    std::puts("kernel synchronization tests passed");
}
