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

	inline std::uint16_t read_u16(const std::byte *buf) {
		std::uint16_t v;
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(&v, buf, 2);
		}
		else {
			v = static_cast<std::uint16_t>(
				std::to_integer<unsigned>(buf[0]) | (std::to_integer<unsigned>(buf[1]) << 8));
		}
		return v;
	}

	inline std::uint32_t read_u32(const std::byte *buf) {
		std::uint32_t v;
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(&v, buf, 4);
		}
		else {
			v = std::to_integer<std::uint32_t>(buf[0]) |
				(std::to_integer<std::uint32_t>(buf[1]) << 8) |
				(std::to_integer<std::uint32_t>(buf[2]) << 16) |
				(std::to_integer<std::uint32_t>(buf[3]) << 24);
		}
		return v;
	}

	inline std::uint64_t read_u64(const std::byte *buf) {
		std::uint64_t v;
		if constexpr (std::endian::native == std::endian::little) {
			std::memcpy(&v, buf, 8);
		}
		else {
			v = 0;
			for (int i = 0; i < 8; ++i)
				v |= std::to_integer<std::uint64_t>(buf[i]) << (8 * i);
		}
		return v;
	}

	inline std::uint16_t read_u16(std::streambuf *sb) {
		std::byte buf[2];
		sb->sgetn(reinterpret_cast<char *>(buf), 2);
		return read_u16(buf);
	}

	inline std::uint32_t read_u32(std::streambuf *sb) {
		std::byte buf[4];
		sb->sgetn(reinterpret_cast<char *>(buf), 4);
		return read_u32(buf);
	}

	inline std::uint64_t read_u64(std::streambuf *sb) {
		std::byte buf[8];
		sb->sgetn(reinterpret_cast<char *>(buf), 8);
		return read_u64(buf);
	}

} // namespace zip_xx::detail
