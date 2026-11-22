#ifndef GBA_MEMORY_HPP
#define GBA_MEMORY_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace gba::mem {

template <typename T>
concept BusWord = std::same_as<std::remove_cv_t<T>, uint8_t> ||
                  std::same_as<std::remove_cv_t<T>, uint16_t> ||
                  std::same_as<std::remove_cv_t<T>, uint32_t>;

enum class Region : uint8_t {
  Bios = 0x0,
  Ewram = 0x2,
  Iwram = 0x3,
  Io = 0x4,
  PaletteRam = 0x5,
  Vram = 0x6,
  Oam = 0x7,
  Cart0 = 0x8,
  Cart0Ex = 0x9,
  Cart1 = 0xA,
  Cart1Ex = 0xB,
  Cart2 = 0xC,
  Cart2Ex = 0xD,
  CartSram = 0xE,
  CartSramMirror = 0xF,
};

inline constexpr uint32_t BaseBios = 0x00000000;
inline constexpr uint32_t BaseEwram = 0x02000000;
inline constexpr uint32_t BaseIwram = 0x03000000;
inline constexpr uint32_t BaseIo = 0x04000000;
inline constexpr uint32_t BasePaletteRam = 0x05000000;
inline constexpr uint32_t BaseVram = 0x06000000;
inline constexpr uint32_t BaseOam = 0x07000000;
inline constexpr uint32_t BaseCart0 = 0x08000000;
inline constexpr uint32_t BaseCartSram = 0x0E000000;

inline constexpr size_t SizeBios = 0x00004000;
inline constexpr size_t SizeEwram = 0x00040000;
inline constexpr size_t SizeIwram = 0x00008000;
inline constexpr size_t SizeIo = 0x00000400;
inline constexpr size_t SizePaletteRam = 0x00000400;
inline constexpr size_t SizeVram = 0x00018000;
inline constexpr size_t SizeOam = 0x00000400;
inline constexpr size_t SizeSramDefault = 0x00008000;

inline constexpr uint32_t OffsetMask = 0x00FFFFFF;
inline constexpr unsigned BaseShift = 24;

[[nodiscard]] constexpr Region region_of(uint32_t address) noexcept {
  return static_cast<Region>((address >> BaseShift) & 0xF);
}

[[nodiscard]] constexpr uint32_t offset_of(uint32_t address) noexcept {
  return address & OffsetMask;
}

template <BusWord T>
[[nodiscard]] inline T load_from(const uint8_t *base, size_t max_size,
                                 uint32_t offset) noexcept {
  if (static_cast<size_t>(offset) + sizeof(T) > max_size) {
    return T{0};
  }
  T value{};
  std::memcpy(&value, base + offset, sizeof(T));
  return value;
}

template <BusWord T>
inline void store_to(uint8_t *base, size_t max_size, uint32_t offset,
                     T value) noexcept {
  if (static_cast<size_t>(offset) + sizeof(T) > max_size) {
    return;
  }
  std::memcpy(base + offset, &value, sizeof(T));
}

class AddressSpace {
public:
  using IoWriteHook = void (*)(void *user_data, uint32_t io_offset,
                               uint32_t value, size_t width);
  using IoReadHook = uint32_t (*)(void *user_data, uint32_t io_offset,
                                  size_t width);

  alignas(4) std::array<uint8_t, SizeBios> bios{};
  alignas(4) std::array<uint8_t, SizeEwram> ewram{};
  alignas(4) std::array<uint8_t, SizeIwram> iwram{};
  alignas(4) std::array<uint8_t, SizeIo> io{};
  alignas(4) std::array<uint8_t, SizePaletteRam> palette{};
  alignas(4) std::array<uint8_t, SizeVram> vram{};
  alignas(4) std::array<uint8_t, SizeOam> oam{};
  std::vector<uint8_t> rom;
  std::vector<uint8_t> sram;

  bool mirror_ram = true;
  void *hook_user = nullptr;
  IoReadHook on_io_read = nullptr;
  IoWriteHook on_io_write = nullptr;

  explicit AddressSpace(size_t sram_size = SizeSramDefault)
      : sram(sram_size, 0xFF) {}

  template <BusWord T> [[nodiscard]] T read(uint32_t address) const noexcept {
    const uint32_t off = offset_of(address);

    switch (region_of(address)) {
    case Region::Bios:
      return load_from<T>(bios.data(), bios.size(), off);

    case Region::Ewram: {
      const uint32_t m_off = mirror_ram ? (off & (SizeEwram - 1)) : off;
      return load_from<T>(ewram.data(), ewram.size(), m_off);
    }

    case Region::Iwram: {
      const uint32_t m_off = mirror_ram ? (off & (SizeIwram - 1)) : off;
      return load_from<T>(iwram.data(), iwram.size(), m_off);
    }

    case Region::Io: {
      if (on_io_read) {
        return static_cast<T>(on_io_read(hook_user, off, sizeof(T)));
      }
      return load_from<T>(io.data(), io.size(), off);
    }

    case Region::PaletteRam: {
      const uint32_t m_off = mirror_ram ? (off & (SizePaletteRam - 1)) : off;
      return load_from<T>(palette.data(), palette.size(), m_off);
    }

    case Region::Vram: {
      uint32_t v_off = off;
      if (v_off >= 0x20000) {
        v_off %= SizeVram;
      } else if (v_off >= SizeVram) {
        v_off -= 0x8000;
      }
      return load_from<T>(vram.data(), vram.size(), v_off);
    }

    case Region::Oam: {
      const uint32_t m_off = mirror_ram ? (off & (SizeOam - 1)) : off;
      return load_from<T>(oam.data(), oam.size(), m_off);
    }

    case Region::Cart0:
    case Region::Cart0Ex:
    case Region::Cart1:
    case Region::Cart1Ex:
    case Region::Cart2:
    case Region::Cart2Ex: {
      if (rom.empty()) {
        return T{0};
      }
      const uint32_t full_cart_off =
          ((static_cast<uint32_t>(region_of(address)) & 1u) ? 0x01000000u
                                                            : 0u) +
          off;
      return load_from<T>(rom.data(), rom.size(), full_cart_off);
    }

    case Region::CartSram:
    case Region::CartSramMirror: {
      if (sram.empty()) {
        return T{0};
      }
      const size_t s_off = static_cast<size_t>(off) % sram.size();
      return load_from<T>(sram.data(), sram.size(), s_off);
    }
    }

    return T{0};
  }

  template <BusWord T> void write(uint32_t address, T value) noexcept {
    const uint32_t off = offset_of(address);

    switch (region_of(address)) {
    case Region::Bios:
    case Region::Cart0:
    case Region::Cart0Ex:
    case Region::Cart1:
    case Region::Cart1Ex:
    case Region::Cart2:
    case Region::Cart2Ex:
      return;

    case Region::Ewram: {
      const uint32_t m_off = mirror_ram ? (off & (SizeEwram - 1)) : off;
      store_to<T>(ewram.data(), ewram.size(), m_off, value);
      return;
    }

    case Region::Iwram: {
      const uint32_t m_off = mirror_ram ? (off & (SizeIwram - 1)) : off;
      store_to<T>(iwram.data(), iwram.size(), m_off, value);
      return;
    }

    case Region::Io: {
      store_to<T>(io.data(), io.size(), off, value);
      if (on_io_write) {
        on_io_write(hook_user, off, static_cast<uint32_t>(value), sizeof(T));
      }
      return;
    }

    case Region::PaletteRam: {
      const uint32_t m_off = mirror_ram ? (off & (SizePaletteRam - 1)) : off;
      store_to<T>(palette.data(), palette.size(), m_off, value);
      return;
    }

    case Region::Vram: {
      uint32_t v_off = off;
      if (v_off >= 0x20000) {
        v_off %= SizeVram;
      } else if (v_off >= SizeVram) {
        v_off -= 0x8000;
      }
      store_to<T>(vram.data(), vram.size(), v_off, value);
      return;
    }

    case Region::Oam: {
      const uint32_t m_off = mirror_ram ? (off & (SizeOam - 1)) : off;
      store_to<T>(oam.data(), oam.size(), m_off, value);
      return;
    }

    case Region::CartSram:
    case Region::CartSramMirror: {
      if (sram.empty()) {
        return;
      }
      const size_t s_off = static_cast<size_t>(off) % sram.size();
      store_to<T>(sram.data(), sram.size(), s_off, value);
      return;
    }
    }
  }
};

} // namespace gba::mem
#endif // GBA_MEMORY_HPP
