#include "records.h"
#include "constants.h"
#include "endian.h"
#include <algorithm>

namespace zip_xx::detail {

	// Local file header layout (30 fixed bytes):
	//   signature         4
	//   version needed    2
	//   gp flags          2
	//   compression       2
	//   mod time          2
	//   mod date          2
	//   crc-32            4   (zero: data descriptor carries real value)
	//   compressed size   4   (zero: data descriptor)
	//   uncompressed size 4   (zero: data descriptor)
	//   filename length   2
	//   extra length      2
	//   filename          n
	//
	// Extra fields (29 bytes):
	//   ZIP64 (id=0x0001, data=16):  4 header + 8 uncompressed + 8 compressed = 20
	//   ext stamp (id=0x5455, data=5): 4 header + 1 flags + 4 mtime             =  9
	std::uint64_t write_local_header(std::streambuf *sb, const entry_record &rec) {
		constexpr std::uint16_t zip64_data_size = 16;
		constexpr std::uint16_t ext_stamp_data_size = 5;
		constexpr std::uint16_t extra_len = 4 + zip64_data_size + 4 + ext_stamp_data_size; // 29

		auto name_len = static_cast<std::uint16_t>(rec.name.size());

		// Fixed fields: 30 bytes
		write_u32(sb, sig_local_header);
		write_u16(sb, version_needed);
		write_u16(sb, rec.general_purpose_flags);
		write_u16(sb, rec.compression_method);
		write_u16(sb, rec.dos_time);
		write_u16(sb, rec.dos_date);
		write_u32(sb, 0); // crc-32
		write_u32(sb, 0); // compressed size
		write_u32(sb, 0); // uncompressed size
		write_u16(sb, name_len);
		write_u16(sb, extra_len);

		// Filename
		sb->sputn(rec.name.data(), name_len);

		// ZIP64 extra field: 20 bytes
		write_u16(sb, xfid_zip64);
		write_u16(sb, zip64_data_size);
		write_u64(sb, 0); // uncompressed size
		write_u64(sb, 0); // compressed size

		// Extended timestamp extra field: 9 bytes
		write_u16(sb, xfid_ext_stamp);
		write_u16(sb, ext_stamp_data_size);
		sb->sputc(static_cast<char>(ext_stamp_has_mtime));
		write_u32(sb, rec.unix_mtime);

		return 30 + name_len + extra_len;
	}

	// Data descriptor layout (24 bytes):
	//   signature         4
	//   crc-32            4
	//   compressed size   8   (ZIP64: 8-byte field)
	//   uncompressed size 8   (ZIP64: 8-byte field)
	std::uint64_t write_data_descriptor(std::streambuf *sb, std::uint32_t crc,
		std::uint64_t compressed, std::uint64_t uncompressed) {
		write_u32(sb, sig_data_descriptor);
		write_u32(sb, crc);
		write_u64(sb, compressed);
		write_u64(sb, uncompressed);
		return 24;
	}

	// Central directory entry layout (46 fixed bytes):
	//   signature             4
	//   version made by       2
	//   version needed        2
	//   gp flags              2
	//   compression           2
	//   mod time              2
	//   mod date              2
	//   crc-32                4
	//   compressed size       4   (0xFFFFFFFF: ZIP64 sentinel)
	//   uncompressed size     4   (0xFFFFFFFF: ZIP64 sentinel)
	//   filename length       2
	//   extra length          2
	//   comment length        2
	//   disk number start     2   (0: single disk)
	//   internal attrs        2   (0)
	//   external attrs        4
	//   local header offset   4   (0xFFFFFFFF: ZIP64 sentinel)
	//   filename              n
	//
	// Extra fields (37 bytes):
	//   ZIP64 (id=0x0001, data=24): 4 + 8 uncompressed + 8 compressed + 8 offset = 28
	//   ext stamp (id=0x5455, data=5): 4 + 1 flags + 4 mtime                     =  9
	std::uint64_t write_central_dir_entry(std::streambuf *sb, const entry_record &rec) {
		constexpr std::uint16_t zip64_data_size = 24;
		constexpr std::uint16_t ext_stamp_data_size = 5;
		constexpr std::uint16_t extra_len = 4 + zip64_data_size + 4 + ext_stamp_data_size; // 37

		auto name_len = static_cast<std::uint16_t>(rec.name.size());
		auto comment_len = static_cast<std::uint16_t>(rec.comment.size());

		// Fixed fields: 46 bytes
		write_u32(sb, sig_central_dir);
		write_u16(sb, version_made_by);
		write_u16(sb, version_needed);
		write_u16(sb, rec.general_purpose_flags);
		write_u16(sb, rec.compression_method);
		write_u16(sb, rec.dos_time);
		write_u16(sb, rec.dos_date);
		write_u32(sb, rec.crc32);
		write_u32(sb, sentinel32); // compressed size
		write_u32(sb, sentinel32); // uncompressed size
		write_u16(sb, name_len);
		write_u16(sb, extra_len);
		write_u16(sb, comment_len);
		write_u16(sb, 0); // disk number start
		write_u16(sb, 0); // internal file attributes
		write_u32(sb, external_attrs_file);
		write_u32(sb, sentinel32); // local header offset

		// Filename
		sb->sputn(rec.name.data(), name_len);

		// ZIP64 extra field: 28 bytes
		write_u16(sb, xfid_zip64);
		write_u16(sb, zip64_data_size);
		write_u64(sb, rec.uncompressed_size);
		write_u64(sb, rec.compressed_size);
		write_u64(sb, rec.local_header_offset);

		// Extended timestamp extra field: 9 bytes (mtime only in central directory)
		write_u16(sb, xfid_ext_stamp);
		write_u16(sb, ext_stamp_data_size);
		sb->sputc(static_cast<char>(ext_stamp_has_mtime));
		write_u32(sb, rec.unix_mtime);

		// Comment
		if (comment_len > 0)
			sb->sputn(rec.comment.data(), comment_len);

		return 46 + name_len + extra_len + comment_len;
	}

	// ZIP64 end of central directory record layout (56 bytes):
	//   signature                       4
	//   size of remaining record        8   (= 44: bytes after this field)
	//   version made by                 2
	//   version needed                  2
	//   disk number                     4   (0)
	//   disk with central dir           4   (0)
	//   entries on this disk            8
	//   total entries                   8
	//   size of central directory       8
	//   offset of central directory     8
	std::uint64_t write_zip64_eocd(std::streambuf *sb, std::uint64_t entry_count,
		std::uint64_t central_dir_size, std::uint64_t central_dir_offset) {
		constexpr std::uint64_t record_body_size = 44;

		write_u32(sb, sig_zip64_eocd);
		write_u64(sb, record_body_size);
		write_u16(sb, version_made_by);
		write_u16(sb, version_needed);
		write_u32(sb, 0); // disk number
		write_u32(sb, 0); // disk with central dir
		write_u64(sb, entry_count);
		write_u64(sb, entry_count);
		write_u64(sb, central_dir_size);
		write_u64(sb, central_dir_offset);

		return 4 + 8 + record_body_size; // 56
	}

	// ZIP64 end of central directory locator layout (20 bytes):
	//   signature                       4
	//   disk with ZIP64 EOCD            4   (0)
	//   offset of ZIP64 EOCD            8
	//   total disks                     4   (1)
	std::uint64_t write_zip64_eocd_locator(std::streambuf *sb, std::uint64_t zip64_eocd_offset) {
		write_u32(sb, sig_zip64_eocd_locator);
		write_u32(sb, 0); // disk with ZIP64 EOCD
		write_u64(sb, zip64_eocd_offset);
		write_u32(sb, 1); // total disks
		return 20;
	}

	// Standard end of central directory record layout (22 + comment bytes):
	//   signature                       4
	//   disk number                     2   (0)
	//   disk with central dir           2   (0)
	//   entries on this disk            2   (clamped to 0xFFFF)
	//   total entries                   2   (clamped to 0xFFFF)
	//   size of central directory       4   (0xFFFFFFFF: ZIP64 sentinel)
	//   offset of central directory     4   (0xFFFFFFFF: ZIP64 sentinel)
	//   comment length                  2
	//   comment                         n
	std::uint64_t write_eocd(std::streambuf *sb, std::uint64_t entry_count,
		std::string_view comment) {
		auto clamped = static_cast<std::uint16_t>(
			std::min(entry_count, static_cast<std::uint64_t>(sentinel16)));
		auto comment_len = static_cast<std::uint16_t>(
			std::min(comment.size(), static_cast<std::size_t>(sentinel16)));

		write_u32(sb, sig_eocd);
		write_u16(sb, 0);		   // disk number
		write_u16(sb, 0);		   // disk with central dir
		write_u16(sb, clamped);	   // entries on this disk
		write_u16(sb, clamped);	   // total entries
		write_u32(sb, sentinel32); // size of central dir
		write_u32(sb, sentinel32); // offset of central dir
		write_u16(sb, comment_len);
		if (comment_len > 0)
			sb->sputn(comment.data(), comment_len);

		return 22 + comment_len;
	}

} // namespace zip_xx::detail
