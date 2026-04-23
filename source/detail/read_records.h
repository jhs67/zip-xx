#pragma once
#include <zip-xx/zip-xx.h>
#include <cstdint>
#include <streambuf>

namespace zip_xx::detail {

	struct eocd_info {
		std::uint64_t entry_count;
		std::uint64_t central_dir_size;
		std::uint64_t central_dir_offset;
	};

	// Locates and parses the end-of-central-directory records. Supports both
	// standard and ZIP64 EOCDs. Throws std::runtime_error if none is found.
	eocd_info find_eocd(std::streambuf *sb);

	// Parses one central directory entry at the current stream position and
	// advances past it. Throws std::runtime_error on a bad signature.
	zip_xx::zip_entry parse_central_dir_entry(std::streambuf *sb);

	// Returns the file offset of the first compressed (or stored) data byte for
	// the entry whose local header starts at local_header_offset.
	std::uint64_t read_local_header_data_offset(std::streambuf *sb,
		std::uint64_t local_header_offset);

} // namespace zip_xx::detail
