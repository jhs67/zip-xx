#include "zip-xx/zip-xx.h"
#include "detail/constants.h"
#include <array>
#include "detail/crc.h"
#include "detail/deflate.h"
#include "detail/entry_record.h"
#include "detail/level_map.h"
#include "detail/records.h"
#include "detail/timestamps.h"
#include <optional>
#include <stdexcept>
#include <vector>

namespace zip_xx {

	struct zip_streambuf::impl {
		explicit impl(std::streambuf *sb) : backing(sb) {}

		std::streambuf *backing;
		std::array<char, 64 * 1024> write_buf;
		std::uint64_t current_offset = 0;
		bool closed = false;
		bool active = false;

		detail::entry_record current_entry;
		compression_t current_compression = compression_t::deflate;
		level_t current_level = level_t::normal;

		detail::crc_accumulator crc;
		std::optional<detail::deflate_stream> deflater;

		std::vector<detail::entry_record> entries;
	};

	zip_streambuf::zip_streambuf(std::ostream &o) : zip_streambuf(o.rdbuf()) {}

	zip_streambuf::zip_streambuf(std::streambuf *sb) : p_(std::make_unique<impl>(sb)) {
		setp(p_->write_buf.data(), p_->write_buf.data() + p_->write_buf.size());
	}

	zip_streambuf::~zip_streambuf() {
		if (!p_->closed) {
			try {
				close();
			} catch (...) {
				// Suppress: destructors must not throw.
				// Call close() explicitly to detect errors.
			}
		}
	}

	zip_streambuf &zip_streambuf::begin_entry(std::string name, std::string comment, stamp_t stamp,
		compression_t compression, level_t level) {
		if (p_->active)
			throw std::logic_error("zip_streambuf: begin_entry called while entry is active");
		if (p_->closed)
			throw std::logic_error("zip_streambuf: begin_entry called on closed archive");

		auto [dos_time, dos_date] = detail::to_dos_datetime(stamp);

		p_->current_entry = {};
		p_->current_entry.name = std::move(name);
		p_->current_entry.comment = std::move(comment);
		p_->current_entry.dos_time = dos_time;
		p_->current_entry.dos_date = dos_date;
		p_->current_entry.unix_mtime = detail::to_unix_time(stamp);
		p_->current_entry.compression_method =
			compression == compression_t::deflate ? detail::method_deflate : detail::method_store;
		p_->current_entry.general_purpose_flags =
			static_cast<std::uint16_t>(detail::flag_data_descriptor | detail::flag_utf8);
		p_->current_entry.local_header_offset = p_->current_offset;

		p_->current_compression = compression;
		p_->current_level = level;
		p_->crc.reset();

		if (compression == compression_t::deflate)
			p_->deflater.emplace(detail::to_zlib_level(level));

		p_->current_offset += detail::write_local_header(p_->backing, p_->current_entry);
		setp(pbase(), epptr());
		p_->active = true;
		return *this;
	}

	zip_streambuf &zip_streambuf::end_entry() {
		if (!p_->active)
			throw std::logic_error("zip_streambuf: end_entry called with no active entry");
		if (p_->closed)
			throw std::logic_error("zip_streambuf: end_entry called on closed archive");

		flush_put_buffer();

		if (p_->deflater) {
			auto final_bytes = p_->deflater->compress(nullptr, 0, p_->backing, true);
			p_->current_entry.compressed_size += final_bytes;
			p_->current_offset += final_bytes;
			p_->deflater.reset();
		}

		p_->current_entry.crc32 = p_->crc.value();
		p_->current_offset += detail::write_data_descriptor(p_->backing, p_->current_entry.crc32,
			p_->current_entry.compressed_size, p_->current_entry.uncompressed_size);

		p_->entries.push_back(p_->current_entry);
		p_->active = false;
		return *this;
	}

	void zip_streambuf::close(std::string_view archive_comment) {
		if (p_->active)
			end_entry();
		if (p_->closed)
			throw std::logic_error("zip_streambuf: close called on already-closed archive");

		auto central_dir_offset = p_->current_offset;
		std::uint64_t central_dir_size = 0;
		for (const auto &rec : p_->entries) {
			auto n = detail::write_central_dir_entry(p_->backing, rec);
			central_dir_size += n;
			p_->current_offset += n;
		}

		auto zip64_eocd_offset = p_->current_offset;
		p_->current_offset += detail::write_zip64_eocd(p_->backing, p_->entries.size(),
			central_dir_size, central_dir_offset);
		p_->current_offset += detail::write_zip64_eocd_locator(p_->backing, zip64_eocd_offset);
		p_->current_offset += detail::write_eocd(p_->backing, p_->entries.size(), archive_comment);

		p_->backing->pubsync();
		p_->closed = true;
	}

	bool zip_streambuf::is_open() const {
		return !p_->closed;
	}

	bool zip_streambuf::entry_active() const {
		return p_->active;
	}

	std::string_view zip_streambuf::entry_name() const {
		return p_->current_entry.name;
	}

	compression_t zip_streambuf::entry_compression() const {
		return p_->current_compression;
	}

	level_t zip_streambuf::entry_level() const {
		return p_->current_level;
	}

	std::uint64_t zip_streambuf::entry_uncompressed_bytes() const {
		return p_->current_entry.uncompressed_size;
	}

	std::uint64_t zip_streambuf::entry_compressed_bytes() const {
		return p_->current_entry.compressed_size;
	}

	std::uint64_t zip_streambuf::current_offset() const {
		return p_->current_offset;
	}

	void zip_streambuf::set_offset(std::uint64_t offset) {
		p_->current_offset = offset;
	}

	std::streamsize zip_streambuf::xsputn(const char_type *s, std::streamsize n) {
		if (!p_->active)
			return 0;

		auto bytes = reinterpret_cast<const std::byte *>(s);
		auto len = static_cast<std::size_t>(n);

		p_->crc.update(bytes, len);

		std::size_t written;
		if (p_->deflater) {
			written = p_->deflater->compress(bytes, len, p_->backing, false);
		}
		else {
			written = static_cast<std::size_t>(p_->backing->sputn(s, n));
		}

		p_->current_offset += written;
		p_->current_entry.compressed_size += written;
		p_->current_entry.uncompressed_size += len;

		return n;
	}

	void zip_streambuf::flush_put_buffer() {
		if (pptr() > pbase()) {
			xsputn(pbase(), pptr() - pbase());
			setp(pbase(), epptr());
		}
	}

	zip_streambuf::int_type zip_streambuf::overflow(int_type c) {
		flush_put_buffer();
		if (c != traits_type::eof()) {
			*pptr() = traits_type::to_char_type(c);
			pbump(1);
		}
		return traits_type::not_eof(c);
	}

	int zip_streambuf::sync() {
		flush_put_buffer();
		return p_->backing->pubsync();
	}

} // namespace zip_xx
