#ifndef ALICE_TEST_GBA_BIOS_CPP
#define ALICE_TEST_GBA_BIOS_CPP

#include <cassert>
#include <cstdint>

#include "gba/bios.hpp"
#include "gba/memory.hpp"

int main() {
  using namespace gba::bios;
  using namespace gba::mem;

  const DivResult d = div_swi(20, 3);
  assert(d.quot == 6);
  assert(d.rem == 2);
  assert(d.abs_quot == 6);

  const DivResult d_zero = div_swi(10, 0);
  assert(d_zero.quot == 1);
  assert(d_zero.rem == 10);

  assert(sqrt_swi(0u) == 0);
  assert(sqrt_swi(256u) == 16);
  assert(sqrt_swi(10000u) == 100);

  assert(arctan2_swi(100, 0) == 0x0000u);
  assert(arctan2_swi(0, 100) == 0x4000u);
  assert(arctan2_swi(-100, 0) == 0x8000u);

  AddressSpace bus;
  const uint32_t src = BaseEwram + 0x100u;
  const uint32_t dst = BaseEwram + 0x200u;
  bus.write<uint32_t>(src, (4u << 8u) | 0x10u);
  bus.write<uint8_t>(src + 4u, 0x00u);
  bus.write<uint8_t>(src + 5u, 'A');
  bus.write<uint8_t>(src + 6u, 'B');
  bus.write<uint8_t>(src + 7u, 'C');
  bus.write<uint8_t>(src + 8u, 'D');

  lz77_uncompress(bus, src, dst, 1);
  assert(bus.read<uint8_t>(dst + 0u) == 'A');
  assert(bus.read<uint8_t>(dst + 1u) == 'B');
  assert(bus.read<uint8_t>(dst + 2u) == 'C');
  assert(bus.read<uint8_t>(dst + 3u) == 'D');

  register_ram_reset(bus, 0x01u);
  assert(bus.read<uint8_t>(dst) == 0u);

  return 0;
}

#endif // ALICE_TEST_GBA_BIOS_CPP
