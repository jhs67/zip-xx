#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <streambuf>

namespace zip_xx::detail {

	inline void write_u16(std::byte *buf, std::uint16_t v) {
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(buf, &v, 2);
		}
		else {
			buf[0] = static_cast<std::byte>(v & 0xFF);
			buf[1] = static_cast<std::byte>((v >> 8) & 0xFF);
		}
	}

	inline void write_u32(std::byte *buf, std::uint32_t v) {
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(buf, &v, 4);
		}
		else {
			buf[0] = static_cast<std::byte>(v & 0xFF);
			buf[1] = static_cast<std::byte>((v >> 8) & 0xFF);
			buf[2] = static_cast<std::byte>((v >> 16) & 0xFF);
			buf[3] = static_cast<std::byte>((v >> 24) & 0xFF);
		}
	}

	inline void write_u64(std::byte *buf, std::uint64_t v) {
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(buf, &v, 8);
		}
		else {
			for (int i = 0; i < 8; ++i)
				buf[i] = static_cast<std::byte>((v >> (8 * i)) & 0xFF);
		}
	}

	inline void write_u16(std::streambuf *sb, std::uint16_t v) {
		std::byte buf[2];
		write_u16(buf, v);
		sb->sputn(reinterpret_cast<const char *>(buf), 2);
	}

	inline void write_u32(std::streambuf *sb, std::uint32_t v) {
		std::byte buf[4];
		write_u32(buf, v);
		sb->sputn(reinterpret_cast<const char *>(buf), 4);
	}

	inline void write_u64(std::streambuf *sb, std::uint64_t v) {
		std::byte buf[8];
		write_u64(buf, v);
		sb->sputn(reinterpret_cast<const char *>(buf), 8);
	}

} // namespace zip_xx::detail
