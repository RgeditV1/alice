#ifndef ALICE_GBA_BIOS_HPP
#define ALICE_GBA_BIOS_HPP

#include <cmath>
#include <cstdint>
#include <cstring>
#include <numbers>

#include "gba/memory.hpp"

namespace gba::bios {

inline constexpr uint32_t GbaBiosChecksum = 0xBAAE187Fu;
inline constexpr uint32_t GbaDsBiosChecksum = 0xBAAE1880u;

struct DivResult {
  int32_t quot{};
  int32_t rem{};
  int32_t abs_quot{};
};

[[nodiscard]] inline uint32_t checksum_words32(const uint8_t *data,
                                               size_t byte_size) noexcept {
  if (!data) {
    return 0u;
  }
  uint32_t sum = 0u;
  for (size_t i = 0u; i + 3u < byte_size; i += 4u) {
    uint32_t word = 0u;
    std::memcpy(&word, data + i, sizeof(word));
    sum += word;
  }
  return sum;
}

[[nodiscard]] inline DivResult div_swi(int32_t num, int32_t denom) noexcept {
  if (denom == 0) {
    const int32_t q = (num < 0) ? -1 : 1;
    return DivResult{q, num, 1};
  }
  if (denom == -1 && num == INT32_MIN) {
    return DivResult{INT32_MIN, 0, INT32_MIN};
  }
  const auto q = static_cast<int32_t>(num / denom);
  const auto r = static_cast<int32_t>(num % denom);
  const uint64_t abs_u =
      (q < 0) ? (static_cast<uint64_t>(~static_cast<uint32_t>(q)) + 1u)
              : static_cast<uint64_t>(q);
  return DivResult{q, r, static_cast<int32_t>(abs_u)};
}

[[nodiscard]] inline int32_t sqrt_swi(uint32_t x) noexcept {
  if (x == 0u) {
    return 0;
  }
  uint32_t upper = x;
  uint32_t bound = 1u;
  while (bound < upper && bound < 0x80000000u) {
    upper >>= 1u;
    bound <<= 1u;
  }
  while (true) {
    upper = x;
    uint32_t accum = 0u;
    uint32_t lower = bound;
    while (true) {
      const uint32_t old_lower = lower;
      if (lower <= (upper >> 1u) && lower < 0x80000000u) {
        lower <<= 1u;
      }
      if (old_lower >= (upper >> 1u)) {
        break;
      }
    }
    while (true) {
      accum <<= 1u;
      if (upper >= lower) {
        ++accum;
        upper -= lower;
      }
      if (lower == bound) {
        break;
      }
      lower >>= 1u;
    }
    const uint32_t old_bound = bound;
    bound = (bound + accum) >> 1u;
    if (bound >= old_bound) {
      bound = old_bound;
      break;
    }
  }
  return static_cast<int32_t>(bound);
}

[[nodiscard]] inline int16_t arctan_swi(int32_t i) noexcept {
  const auto i64 = static_cast<int64_t>(i);
  const int64_t a = -((i64 * i64) >> 14);
  int64_t b = ((0xA9ll * a) >> 14) + 0x390ll;
  b = ((b * a) >> 14) + 0x91Cll;
  b = ((b * a) >> 14) + 0xFB6ll;
  b = ((b * a) >> 14) + 0x16AAll;
  b = ((b * a) >> 14) + 0x2081ll;
  b = ((b * a) >> 14) + 0x3651ll;
  b = ((b * a) >> 14) + 0xA2F9ll;
  return static_cast<int16_t>((i64 * b) >> 16);
}

[[nodiscard]] inline uint16_t arctan2_swi(int32_t x, int32_t y) noexcept {
  if (y == 0) {
    return (x >= 0) ? 0x0000u : 0x8000u;
  }
  if (x == 0) {
    return (y > 0) ? 0x4000u : 0xC000u;
  }
  const auto x64 = static_cast<int64_t>(x);
  const auto y64 = static_cast<int64_t>(y);
  if (y > 0) {
    if (x > 0) {
      if (x >= y) {
        const auto ratio = static_cast<int32_t>((y64 << 14) / x64);
        return static_cast<uint16_t>(arctan_swi(ratio));
      }
    } else if (-x64 >= y64) {
      const auto ratio = static_cast<int32_t>((y64 << 14) / x64);
      return static_cast<uint16_t>(arctan_swi(ratio) + 0x8000);
    }
    const auto ratio = static_cast<int32_t>((x64 << 14) / y64);
    return static_cast<uint16_t>(0x4000 - arctan_swi(ratio));
  } else {
    if (x < 0) {
      if (-x64 > -y64) {
        const auto ratio = static_cast<int32_t>((y64 << 14) / x64);
        return static_cast<uint16_t>(arctan_swi(ratio) + 0x8000);
      }
    } else if (x64 >= -y64) {
      const auto ratio = static_cast<int32_t>((y64 << 14) / x64);
      return static_cast<uint16_t>(arctan_swi(ratio) + 0x10000);
    }
    const auto ratio = static_cast<int32_t>((x64 << 14) / y64);
    return static_cast<uint16_t>(0xC000 - arctan_swi(ratio));
  }
}

[[nodiscard]] inline uint32_t midi_key2freq(uint32_t key_base_freq, uint8_t key,
                                            uint8_t fine_tune) noexcept {
  const float exponent = (180.0f - static_cast<float>(key) -
                          (static_cast<float>(fine_tune) / 256.0f)) /
                         12.0f;
  return static_cast<uint32_t>(static_cast<float>(key_base_freq) /
                               std::exp2(exponent));
}

inline void bg_affine_set(gba::mem::AddressSpace &space, uint32_t src_addr,
                          uint32_t dst_addr, int count) noexcept {
  uint32_t src = src_addr;
  uint32_t dst = dst_addr;

  for (int i = 0; i < count; ++i) {
    const float ox =
        static_cast<float>(static_cast<int32_t>(space.read<uint32_t>(src))) /
        256.0f;
    const float oy = static_cast<float>(
                         static_cast<int32_t>(space.read<uint32_t>(src + 4u))) /
                     256.0f;
    const float cx = static_cast<float>(
        static_cast<int16_t>(space.read<uint16_t>(src + 8u)));
    const float cy = static_cast<float>(
        static_cast<int16_t>(space.read<uint16_t>(src + 10u)));
    const float sx = static_cast<float>(static_cast<int16_t>(
                         space.read<uint16_t>(src + 12u))) /
                     256.0f;
    const float sy = static_cast<float>(static_cast<int16_t>(
                         space.read<uint16_t>(src + 14u))) /
                     256.0f;
    const float theta =
        (static_cast<float>(space.read<uint16_t>(src + 16u) >> 8u) / 128.0f) *
        std::numbers::pi_v<float>;
    src += 20u;

    const float cos_t = std::cos(theta);
    const float sin_t = std::sin(theta);
    const float pa = cos_t * sx;
    const float pb = -sin_t * sx;
    const float pc = sin_t * sy;
    const float pd = cos_t * sy;
    const float rx = ox - (pa * cx + pb * cy);
    const float ry = oy - (pc * cx + pd * cy);

    space.write<uint16_t>(dst, static_cast<uint16_t>(static_cast<int16_t>(
                                   std::lround(pa * 256.0f))));
    space.write<uint16_t>(dst + 2u, static_cast<uint16_t>(static_cast<int16_t>(
                                        std::lround(pb * 256.0f))));
    space.write<uint16_t>(dst + 4u, static_cast<uint16_t>(static_cast<int16_t>(
                                        std::lround(pc * 256.0f))));
    space.write<uint16_t>(dst + 6u, static_cast<uint16_t>(static_cast<int16_t>(
                                        std::lround(pd * 256.0f))));
    space.write<uint32_t>(dst + 8u, static_cast<uint32_t>(static_cast<int32_t>(
                                        std::lround(rx * 256.0f))));
    space.write<uint32_t>(dst + 12u, static_cast<uint32_t>(static_cast<int32_t>(
                                         std::lround(ry * 256.0f))));
    dst += 16u;
  }
}

inline void obj_affine_set(gba::mem::AddressSpace &space, uint32_t src_addr,
                           uint32_t dst_addr, int count,
                           uint32_t diff) noexcept {
  uint32_t src = src_addr;
  uint32_t dst = dst_addr;

  for (int i = 0; i < count; ++i) {
    const float sx =
        static_cast<float>(static_cast<int16_t>(space.read<uint16_t>(src))) /
        256.0f;
    const float sy = static_cast<float>(
                         static_cast<int16_t>(space.read<uint16_t>(src + 2u))) /
                     256.0f;
    const float theta =
        (static_cast<float>(space.read<uint16_t>(src + 4u) >> 8u) / 128.0f) *
        std::numbers::pi_v<float>;
    src += 8u;

    const float cos_t = std::cos(theta);
    const float sin_t = std::sin(theta);
    const float pa = cos_t * sx;
    const float pb = -sin_t * sx;
    const float pc = sin_t * sy;
    const float pd = cos_t * sy;

    space.write<uint16_t>(dst, static_cast<uint16_t>(static_cast<int16_t>(
                                   std::lround(pa * 256.0f))));
    space.write<uint16_t>(
        dst + diff,
        static_cast<uint16_t>(static_cast<int16_t>(std::lround(pb * 256.0f))));
    space.write<uint16_t>(
        dst + diff * 2u,
        static_cast<uint16_t>(static_cast<int16_t>(std::lround(pc * 256.0f))));
    space.write<uint16_t>(
        dst + diff * 3u,
        static_cast<uint16_t>(static_cast<int16_t>(std::lround(pd * 256.0f))));
    dst += diff * 4u;
  }
}

inline void lz77_uncompress(gba::mem::AddressSpace &space, uint32_t src_addr,
                            uint32_t dst_addr, int width) noexcept {
  uint32_t src = src_addr;
  uint32_t dst = dst_addr;
  const uint32_t header = space.read<uint32_t>(src);
  int remaining = static_cast<int>((header & 0xFFFFFF00u) >> 8u);
  src += 4u;

  uint8_t block_header = 0u;
  int blocks_remaining = 0;
  uint16_t halfword = 0u;

  while (remaining > 0) {
    if (blocks_remaining > 0) {
      if ((block_header & 0x80u) != 0u) {
        const uint16_t hi = space.read<uint8_t>(src + 1u);
        const uint16_t lo = space.read<uint8_t>(src);
        const uint16_t block = static_cast<uint16_t>(hi | (lo << 8u));
        src += 2u;

        uint32_t disp = dst - static_cast<uint32_t>(block & 0x0FFFu) - 1u;
        int bytes = static_cast<int>((block >> 12u) + 3u);
        while (bytes-- > 0) {
          if (remaining > 0) {
            --remaining;
          }
          if (width == 2) {
            const uint16_t hw = space.read<uint16_t>(disp & ~1u);
            const uint8_t byte =
                static_cast<uint8_t>((hw >> ((disp & 1u) * 8u)) & 0xFFu);
            if ((dst & 1u) != 0u) {
              halfword = static_cast<uint16_t>(
                  halfword | (static_cast<uint16_t>(byte) << 8u));
              space.write<uint16_t>(dst ^ 1u, halfword);
            } else {
              halfword = byte;
            }
          } else {
            const uint8_t byte = space.read<uint8_t>(disp);
            space.write<uint8_t>(dst, byte);
          }
          ++disp;
          ++dst;
        }
      } else {
        const uint8_t byte = space.read<uint8_t>(src);
        ++src;
        if (width == 2) {
          if ((dst & 1u) != 0u) {
            halfword = static_cast<uint16_t>(
                halfword | (static_cast<uint16_t>(byte) << 8u));
            space.write<uint16_t>(dst ^ 1u, halfword);
          } else {
            halfword = byte;
          }
        } else {
          space.write<uint8_t>(dst, byte);
        }
        ++dst;
        --remaining;
      }
      block_header = static_cast<uint8_t>(block_header << 1u);
      --blocks_remaining;
    } else {
      block_header = space.read<uint8_t>(src);
      ++src;
      blocks_remaining = 8;
    }
  }
}

inline void rl_uncompress(gba::mem::AddressSpace &space, uint32_t src_addr,
                          uint32_t dst_addr, int width) noexcept {
  uint32_t src = src_addr;
  uint32_t dst = dst_addr;
  const uint32_t header = space.read<uint32_t>(src & ~3u);
  int remaining = static_cast<int>((header & 0xFFFFFF00u) >> 8u);
  int padding = (4 - remaining) & 0x3;
  src += 4u;
  uint16_t halfword = 0u;

  while (remaining > 0) {
    uint8_t block_header = space.read<uint8_t>(src);
    ++src;
    if ((block_header & 0x80u) != 0u) {
      int count = static_cast<int>(block_header & 0x7Fu) + 3;
      const uint8_t val = space.read<uint8_t>(src);
      ++src;
      while (count-- > 0 && remaining > 0) {
        --remaining;
        if (width == 2) {
          if ((dst & 1u) != 0u) {
            halfword = static_cast<uint16_t>(
                halfword | (static_cast<uint16_t>(val) << 8u));
            space.write<uint16_t>(dst ^ 1u, halfword);
          } else {
            halfword = val;
          }
        } else {
          space.write<uint8_t>(dst, val);
        }
        ++dst;
      }
    } else {
      int count = static_cast<int>(block_header) + 1;
      while (count-- > 0 && remaining > 0) {
        --remaining;
        const uint8_t byte = space.read<uint8_t>(src);
        ++src;
        if (width == 2) {
          if ((dst & 1u) != 0u) {
            halfword = static_cast<uint16_t>(
                halfword | (static_cast<uint16_t>(byte) << 8u));
            space.write<uint16_t>(dst ^ 1u, halfword);
          } else {
            halfword = byte;
          }
        } else {
          space.write<uint8_t>(dst, byte);
        }
        ++dst;
      }
    }
  }

  if (width == 2) {
    if ((dst & 1u) != 0u) {
      --padding;
      ++dst;
    }
    for (; padding > 0; padding -= 2, dst += 2u) {
      space.write<uint16_t>(dst, 0u);
    }
  } else {
    while (padding-- > 0) {
      space.write<uint8_t>(dst, 0u);
      ++dst;
    }
  }
}

inline void diff_unfilter(gba::mem::AddressSpace &space, uint32_t src_addr,
                          uint32_t dst_addr, int in_width,
                          int out_width) noexcept {
  uint32_t src = src_addr & ~3u;
  uint32_t dst = dst_addr;
  const uint32_t header = space.read<uint32_t>(src);
  int remaining = static_cast<int>(header >> 8u);
  src += 4u;

  uint16_t halfword = 0u;
  uint16_t old_val = 0u;

  while (remaining > 0) {
    const uint16_t raw =
        (in_width == 1) ? space.read<uint8_t>(src) : space.read<uint16_t>(src);
    const auto new_val = static_cast<uint16_t>(raw + old_val);

    if (out_width > in_width) {
      halfword = static_cast<uint16_t>((halfword >> 8u) |
                                       (static_cast<uint16_t>(new_val << 8u)));
      if ((src & 1u) != 0u) {
        space.write<uint16_t>(dst, halfword);
        dst += static_cast<uint32_t>(out_width);
        remaining -= out_width;
      }
    } else if (out_width == 1) {
      space.write<uint8_t>(dst, static_cast<uint8_t>(new_val & 0xFFu));
      dst += static_cast<uint32_t>(out_width);
      remaining -= out_width;
    } else {
      space.write<uint16_t>(dst, new_val);
      dst += static_cast<uint32_t>(out_width);
      remaining -= out_width;
    }

    old_val = new_val;
    src += static_cast<uint32_t>(in_width);
  }
}

inline void bit_unpack(gba::mem::AddressSpace &space, uint32_t src_addr,
                       uint32_t dst_addr, uint32_t info_addr) noexcept {
  uint32_t src = src_addr;
  uint32_t dst = dst_addr;
  unsigned source_len = space.read<uint16_t>(info_addr);
  const unsigned source_width = space.read<uint8_t>(info_addr + 2u);
  const unsigned dest_width = space.read<uint8_t>(info_addr + 3u);

  if (source_width != 1u && source_width != 2u && source_width != 4u &&
      source_width != 8u) {
    return;
  }
  if (dest_width != 1u && dest_width != 2u && dest_width != 4u &&
      dest_width != 8u && dest_width != 16u && dest_width != 32u) {
    return;
  }

  const uint32_t bias = space.read<uint32_t>(info_addr + 4u);
  uint8_t in_byte = 0u;
  uint32_t out_word = 0u;
  int bits_remaining = 0;
  int bits_eaten = 0;
  const uint32_t src_mask = (1u << source_width) - 1u;

  while (source_len > 0u || bits_remaining > 0) {
    if (bits_remaining == 0) {
      in_byte = space.read<uint8_t>(src);
      bits_remaining = 8;
      ++src;
      --source_len;
    }
    uint32_t scaled = in_byte & src_mask;
    in_byte = static_cast<uint8_t>(in_byte >> source_width);
    if (scaled != 0u || (bias & 0x80000000u) != 0u) {
      scaled += (bias & 0x7FFFFFFFu);
    }
    bits_remaining -= static_cast<int>(source_width);
    out_word |= (scaled << bits_eaten);
    bits_eaten += static_cast<int>(dest_width);
    if (bits_eaten == 32) {
      space.write<uint32_t>(dst, out_word);
      bits_eaten = 0;
      out_word = 0u;
      dst += 4u;
    }
  }
}

inline void register_ram_reset(gba::mem::AddressSpace &space,
                               uint32_t flags) noexcept {
  if ((flags & 0x01u) != 0u) {
    space.ewram.fill(0u);
  }
  if ((flags & 0x02u) != 0u) {
    for (size_t i = 0u; i + 0x200u < space.iwram.size(); ++i) {
      space.iwram[i] = 0u;
    }
  }
  if ((flags & 0x04u) != 0u) {
    space.palette.fill(0u);
  }
  if ((flags & 0x08u) != 0u) {
    space.vram.fill(0u);
  }
  if ((flags & 0x10u) != 0u) {
    space.oam.fill(0u);
  }
}

} // namespace gba::bios

#endif // ALICE_GBA_BIOS_HPP
