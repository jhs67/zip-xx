#include "zip-xx/zip-xx.h"

namespace zip_xx {

	zip_ostream::zip_ostream(std::ostream &o) : zip_ostream(o.rdbuf()) {}

	zip_ostream::zip_ostream(std::streambuf *sb)
		: detail::zip_ostream_base(sb), std::ostream(&buf_) {}

	zip_ostream &zip_ostream::begin_entry(std::string name, std::string comment, stamp_t stamp,
		compression_t compression, level_t level) {
		buf_.begin_entry(std::move(name), std::move(comment), stamp, compression, level);
		return *this;
	}

	zip_ostream &zip_ostream::end_entry() {
		buf_.end_entry();
		return *this;
	}

	void zip_ostream::close(std::string_view archive_comment) {
		buf_.close(archive_comment);
	}

	bool zip_ostream::is_open() const {
		return buf_.is_open();
	}

	bool zip_ostream::entry_active() const {
		return buf_.entry_active();
	}

	std::string_view zip_ostream::entry_name() const {
		return buf_.entry_name();
	}

	compression_t zip_ostream::entry_compression() const {
		return buf_.entry_compression();
	}

	level_t zip_ostream::entry_level() const {
		return buf_.entry_level();
	}

	std::uint64_t zip_ostream::entry_uncompressed_bytes() const {
		return buf_.entry_uncompressed_bytes();
	}

	std::uint64_t zip_ostream::entry_compressed_bytes() const {
		return buf_.entry_compressed_bytes();
	}

	std::uint64_t zip_ostream::current_offset() const {
		return buf_.current_offset();
	}

	void zip_ostream::set_offset(std::uint64_t offset) {
		buf_.set_offset(offset);
	}

	zip_streambuf *zip_ostream::rdbuf() const {
		return const_cast<zip_streambuf *>(&buf_);
	}

} // namespace zip_xx
