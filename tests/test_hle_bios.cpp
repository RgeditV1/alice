#ifndef ALICE_TEST_HLE_BIOS_CPP
#define ALICE_TEST_HLE_BIOS_CPP

#include <algorithm>
#include <cassert>
#include <cstdint>

#include "gba/hle_bios.hpp"
#include "gba/memory.hpp"

static_assert(gba::bios::hle_bios.size() == gba::mem::SizeBios);
static_assert(gba::bios::hle_bios[0] == 0xd3u);
static_assert(gba::bios::hle_bios[1] == 0x00u);
static_assert(gba::bios::hle_bios[2] == 0x00u);
static_assert(gba::bios::hle_bios[3] == 0xeau);

int main() {
  gba::mem::AddressSpace bus;
  gba::bios::load_into_memory(bus);

  assert(std::equal(bus.bios.begin(), bus.bios.end(),
                    gba::bios::hle_bios.begin()));

  assert(bus.read<uint8_t>(gba::mem::BaseBios + 0u) == 0xd3u);
  assert(bus.read<uint32_t>(gba::mem::BaseBios + 0u) == 0xea0000d3u);

  const uint8_t prev = bus.read<uint8_t>(gba::mem::BaseBios + 0u);
  bus.write<uint8_t>(gba::mem::BaseBios + 0u, 0x00u);
  assert(bus.read<uint8_t>(gba::mem::BaseBios + 0u) == prev);

  return 0;
}

#endif // ALICE_TEST_HLE_BIOS_CPP
