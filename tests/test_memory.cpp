#include "gba/memory.hpp"
#include <cassert>
#include <cstdint>

// Compile Time Checks
static_assert(gba::mem::region_of(0x02001234u) == gba::mem::Region::Ewram);
static_assert(gba::mem::offset_of(0x02001234u) == 0x001234u);
static_assert(gba::mem::region_of(0x04000130u) == gba::mem::Region::Io);

int main() {
  gba::mem::AddressSpace bus;

  bus.write<uint32_t>(gba::mem::BaseEwram + 0x10u, 0x12345678u);
  assert(bus.read<uint32_t>(gba::mem::BaseEwram + 0x10u) == 0x12345678u);
  assert(bus.read<uint16_t>(gba::mem::BaseEwram + 0x10u) == 0x5678u);
  assert(bus.read<uint8_t>(gba::mem::BaseEwram + 0x12u) == 0x34u);

  // Mirror of EWRAM (+256 KB)
  assert(bus.read<uint32_t>(gba::mem::BaseEwram + gba::mem::SizeEwram +
                            0x10u) == 0x12345678u);

  // Register I/O
  bus.write<uint16_t>(gba::mem::BaseIo + 0x00u, 0x0403u);
  assert(bus.read<uint16_t>(gba::mem::BaseIo + 0x00u) == 0x0403u);

  return 0;
}
