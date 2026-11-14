#include "runtime/guest_memory.h"

// Synthetic layout dimensions: no game headers or executable are needed.
static_assert(guest::kHeapBase >= 0x84C10000u);
static_assert(guest::kHeapBase < guest::kHeapEnd);
static_assert(guest::kHeapEnd == guest::kPhysicalBase);
static_assert(guest::kPhysicalEnd <= guest::kApertureBase);

int main() { return 0; }
