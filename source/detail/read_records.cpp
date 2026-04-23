#include "read_records.h"
#include "constants.h"
#include "endian.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <stdexcept>
#include <vector>

namespace zip_xx::detail {

	// Standard EOCD layout (22 fixed bytes):
	//   signature              4
	//   disk number            2
	//   disk with central dir  2
	//   entries on this disk   2
	//   total entries          2   offset 10
	//   central dir size       4   offset 12
	//   central dir offset     4   offset 16
	//   comment length         2   offset 20
	eocd_info find_eocd(std::streambuf *sb) {
		auto file_size =
			static_cast<std::uint64_t>(sb->pubseekoff(0, std::ios_base::end, std::ios_base::in));

		if (file_size < 22)
			throw std::runtime_error("archive too small to contain EOCD");

		// 65535 (max comment) + 22 (fixed EOCD) = 65557
		constexpr std::uint64_t max_scan = 65557;
		auto read_len = std::min(max_scan, file_size);
		auto read_start = file_size - read_len;

		sb->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(read_start)),
			std::ios_base::in);

		std::vector<std::byte> buf(static_cast<std::size_t>(read_len));
		sb->sgetn(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(read_len));

		// Scan backward: first valid EOCD found is the one closest to EOF.
		std::int64_t eocd_buf_pos = -1;
		for (std::int64_t i = static_cast<std::int64_t>(read_len) - 22; i >= 0; --i) {
			if (read_u32(buf.data() + i) != sig_eocd)
				continue;
			auto comment_len = read_u16(buf.data() + i + 20);
			if (static_cast<std::int64_t>(comment_len) ==
				static_cast<std::int64_t>(read_len) - i - 22) {
				eocd_buf_pos = i;
				break;
			}
		}

		if (eocd_buf_pos < 0)
			throw std::runtime_error("no valid end-of-central-directory record found");

		auto eocd_file_offset = read_start + static_cast<std::uint64_t>(eocd_buf_pos);
		const std::byte *eocd = buf.data() + eocd_buf_pos;

		auto entry_count_16 = read_u16(eocd + 10);
		auto central_dir_size_32 = read_u32(eocd + 12);
		auto central_dir_offset_32 = read_u32(eocd + 16);

		if (central_dir_size_32 == sentinel32 || central_dir_offset_32 == sentinel32) {
			// ZIP64 EOCD locator is the 20-byte record immediately before the EOCD.
			if (eocd_file_offset < 20)
				throw std::runtime_error("no room for ZIP64 EOCD locator");

			sb->pubseekpos(
				static_cast<std::streampos>(static_cast<std::streamoff>(eocd_file_offset - 20)),
				std::ios_base::in);

			if (read_u32(sb) != sig_zip64_eocd_locator)
				throw std::runtime_error("ZIP64 EOCD locator signature not found");

			read_u32(sb); // disk with ZIP64 EOCD
			auto zip64_eocd_offset = read_u64(sb);

			// ZIP64 EOCD layout after signature (56 bytes total):
			//   size of remaining record  8
			//   version made by           2
			//   version needed            2
			//   disk number               4
			//   disk with central dir     4
			//   entries on this disk      8
			//   total entries             8
			//   central dir size          8
			//   central dir offset        8
			sb->pubseekpos(
				static_cast<std::streampos>(static_cast<std::streamoff>(zip64_eocd_offset)),
				std::ios_base::in);

			if (read_u32(sb) != sig_zip64_eocd)
				throw std::runtime_error("ZIP64 EOCD signature not found");

			read_u64(sb); // size of remaining record
			read_u16(sb); // version made by
			read_u16(sb); // version needed
			read_u32(sb); // disk number
			read_u32(sb); // disk with central dir
			read_u64(sb); // entries on this disk
			auto entry_count = read_u64(sb);
			auto central_dir_size = read_u64(sb);
			auto central_dir_offset = read_u64(sb);

			return { entry_count, central_dir_size, central_dir_offset };
		}

		return {
			static_cast<std::uint64_t>(entry_count_16),
			static_cast<std::uint64_t>(central_dir_size_32),
			static_cast<std::uint64_t>(central_dir_offset_32),
		};
	}

	// Central directory entry layout (46 fixed bytes):
	//   signature              4
	//   version made by        2
	//   version needed         2
	//   gp flags               2
	//   compression method     2   offset 10
	//   mod time               2   offset 12
	//   mod date               2   offset 14
	//   crc-32                 4   offset 16
	//   compressed size        4   offset 20
	//   uncompressed size      4   offset 24
	//   filename length        2   offset 28
	//   extra length           2   offset 30
	//   comment length         2   offset 32
	//   disk number start      2   offset 34
	//   internal attributes    2   offset 36
	//   external attributes    4   offset 38
	//   local header offset    4   offset 42
	zip_xx::zip_entry parse_central_dir_entry(std::streambuf *sb) {
		if (read_u32(sb) != sig_central_dir)
			throw std::runtime_error("invalid central directory entry signature");

		read_u16(sb); // version made by
		read_u16(sb); // version needed
		read_u16(sb); // gp flags
		auto compression_method = read_u16(sb);
		read_u16(sb); // mod time
		read_u16(sb); // mod date
		read_u32(sb); // crc-32
		auto compressed_size_32 = read_u32(sb);
		auto uncompressed_size_32 = read_u32(sb);
		auto name_len = read_u16(sb);
		auto extra_len = read_u16(sb);
		auto comment_len = read_u16(sb);
		read_u16(sb); // disk number start
		read_u16(sb); // internal attributes
		read_u32(sb); // external attributes
		auto local_header_offset_32 = read_u32(sb);

		std::string name(name_len, '\0');
		sb->sgetn(name.data(), name_len);

		std::vector<std::byte> extra(extra_len);
		if (extra_len > 0)
			sb->sgetn(reinterpret_cast<char *>(extra.data()), extra_len);

		auto compressed_size = static_cast<std::uint64_t>(compressed_size_32);
		auto uncompressed_size = static_cast<std::uint64_t>(uncompressed_size_32);
		auto local_header_offset = static_cast<std::uint64_t>(local_header_offset_32);
		zip_xx::stamp_t timestamp = {};

		// Parse extra field block.
		std::size_t pos = 0;
		while (pos + 4 <= static_cast<std::size_t>(extra_len)) {
			auto id = read_u16(extra.data() + pos);
			auto data_size = read_u16(extra.data() + pos + 2);
			pos += 4;
			if (pos + data_size > static_cast<std::size_t>(extra_len))
				break;

			if (id == xfid_zip64) {
				// Per spec: only the sentinel fields are present, in this order:
				// uncompressed size, compressed size, local header offset.
				std::size_t p = pos;
				if (uncompressed_size_32 == sentinel32 && p + 8 <= pos + data_size) {
					uncompressed_size = read_u64(extra.data() + p);
					p += 8;
				}
				if (compressed_size_32 == sentinel32 && p + 8 <= pos + data_size) {
					compressed_size = read_u64(extra.data() + p);
					p += 8;
				}
				if (local_header_offset_32 == sentinel32 && p + 8 <= pos + data_size) {
					local_header_offset = read_u64(extra.data() + p);
				}
			}
			else if (id == xfid_ext_stamp && data_size >= 1) {
				auto flags = std::to_integer<std::uint8_t>(extra[pos]);
				if ((flags & 0x01u) && data_size >= 5) {
					auto mtime = read_u32(extra.data() + pos + 1);
					timestamp =
						std::chrono::system_clock::from_time_t(static_cast<std::time_t>(mtime));
				}
			}

			pos += data_size;
		}

		std::string comment(comment_len, '\0');
		if (comment_len > 0)
			sb->sgetn(comment.data(), comment_len);

		compression_t compression = compression_t::store;
		if (compression_method == method_deflate)
			compression = compression_t::deflate;

		return zip_xx::zip_entry{
			std::move(name),
			uncompressed_size,
			compressed_size,
			compression,
			timestamp,
			std::move(comment),
			local_header_offset,
		};
	}

	// Local header layout (30 fixed bytes):
	//   signature              4
	//   version needed         2
	//   gp flags               2
	//   compression method     2
	//   mod time               2
	//   mod date               2
	//   crc-32                 4
	//   compressed size        4
	//   uncompressed size      4
	//   filename length        2   offset 26
	//   extra length           2   offset 28
	std::uint64_t read_local_header_data_offset(std::streambuf *sb,
		std::uint64_t local_header_offset) {
		sb->pubseekpos(
			static_cast<std::streampos>(static_cast<std::streamoff>(local_header_offset)),
			std::ios_base::in);

		if (read_u32(sb) != sig_local_header)
			throw std::runtime_error("invalid local file header signature");

		// Skip to filename length (22 bytes between signature and that field).
		std::byte skip[22];
		sb->sgetn(reinterpret_cast<char *>(skip), 22);

		auto name_len = read_u16(sb);
		auto extra_len = read_u16(sb);

		return local_header_offset + 30 + name_len + extra_len;
	}

} // namespace zip_xx::detail
