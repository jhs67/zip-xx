#pragma once
#include "entry_record.h"
#include <cstdint>
#include <streambuf>
#include <string_view>

namespace zip_xx::detail {

	// Each function writes one ZIP record to sb and returns the number of bytes written.

	// Local file header + ZIP64 extra field + extended timestamp extra field.
	// CRC and size fields are zeroed; the data descriptor carries the real values.
	// Returns: 30 + name.size() + 29
	std::uint64_t write_local_header(std::streambuf *sb, const entry_record &rec);

	// ZIP64 data descriptor: signature + CRC-32 + compressed size + uncompressed size.
	// Returns: 24
	std::uint64_t write_data_descriptor(std::streambuf *sb, std::uint32_t crc,
		std::uint64_t compressed, std::uint64_t uncompressed);

	// Central directory file header + ZIP64 extra field + extended timestamp extra field + comment.
	// Compressed size, uncompressed size, and local header offset use ZIP64 sentinels in the
	// fixed fields; real values go in the ZIP64 extra field.
	// Returns: 46 + name.size() + 37 + comment.size()
	std::uint64_t write_central_dir_entry(std::streambuf *sb, const entry_record &rec);

	// ZIP64 end of central directory record.
	// Returns: 56
	std::uint64_t write_zip64_eocd(std::streambuf *sb, std::uint64_t entry_count,
		std::uint64_t central_dir_size, std::uint64_t central_dir_offset);

	// ZIP64 end of central directory locator.
	// Returns: 20
	std::uint64_t write_zip64_eocd_locator(std::streambuf *sb, std::uint64_t zip64_eocd_offset);

	// Standard end of central directory record.
	// Entry count is clamped to 0xFFFF; size and offset fields use ZIP64 sentinels.
	// Returns: 22 + comment.size()
	std::uint64_t write_eocd(std::streambuf *sb, std::uint64_t entry_count,
		std::string_view comment);

} // namespace zip_xx::detail
