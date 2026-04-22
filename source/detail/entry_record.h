#pragma once
#include <cstdint>
#include <string>

namespace zip_xx::detail {

	// All data needed to write one central directory entry.
	// Populated during begin_entry / end_entry, accumulated until close().
	struct entry_record {
		std::string name;
		std::string comment;
		std::uint16_t dos_time = 0;
		std::uint16_t dos_date = 0;
		std::uint32_t unix_mtime = 0;
		std::uint16_t compression_method = 0;
		std::uint16_t general_purpose_flags = 0;
		std::uint32_t crc32 = 0;
		std::uint64_t compressed_size = 0;
		std::uint64_t uncompressed_size = 0;
		std::uint64_t local_header_offset = 0;
	};

} // namespace zip_xx::detail
