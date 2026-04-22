#pragma once
#include <cstdint>

namespace zip_xx::detail {

	// Record signatures
	inline constexpr std::uint32_t sig_local_header = 0x04034b50;
	inline constexpr std::uint32_t sig_central_dir = 0x02014b50;
	inline constexpr std::uint32_t sig_data_descriptor = 0x08074b50;
	inline constexpr std::uint32_t sig_zip64_eocd = 0x06064b50;
	inline constexpr std::uint32_t sig_zip64_eocd_locator = 0x07064b50;
	inline constexpr std::uint32_t sig_eocd = 0x06054b50;

	// Extra field IDs
	inline constexpr std::uint16_t xfid_zip64 = 0x0001;
	inline constexpr std::uint16_t xfid_ext_stamp = 0x5455; // extended timestamp

	// Version needed to extract (ZIP64 = 4.5)
	inline constexpr std::uint16_t version_needed = 45;

	// Version made by: upper byte = OS (0x03 = Unix), lower = spec version (45)
	inline constexpr std::uint16_t version_made_by = 0x0345;

	// General purpose flags
	inline constexpr std::uint16_t flag_data_descriptor = 0x0008; // bit 3
	inline constexpr std::uint16_t flag_utf8 = 0x0800;			  // bit 11

	// Compression methods
	inline constexpr std::uint16_t method_store = 0;
	inline constexpr std::uint16_t method_deflate = 8;

	// External attributes: Unix regular file, mode 0644 (S_IFREG | 0644 = 0x81A4)
	// stored in the high 16 bits of the 32-bit external attributes field
	inline constexpr std::uint32_t external_attrs_file = 0x81A40000;

	// Sentinel values used in the standard EOCD when the real values are in ZIP64 records
	inline constexpr std::uint16_t sentinel16 = 0xFFFF;
	inline constexpr std::uint32_t sentinel32 = 0xFFFFFFFF;

	// Extended timestamp flag: modification time present
	inline constexpr std::uint8_t ext_stamp_has_mtime = 0x01;

} // namespace zip_xx::detail
