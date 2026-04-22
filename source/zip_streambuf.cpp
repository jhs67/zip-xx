#include "zip-xx/zip-xx.h"

namespace zip_xx {

	struct zip_streambuf::impl {
		explicit impl(std::streambuf *sb) : backing(sb) {}

		std::streambuf *backing;
		std::uint64_t current_offset = 0;
		bool closed = false;
		bool active = false;

		std::string name;
		std::string comment;
		compression_t compression = compression_t::deflate;
		level_t level = level_t::normal;
		std::uint64_t uncompressed_bytes = 0;
		std::uint64_t compressed_bytes = 0;
	};

	zip_streambuf::zip_streambuf(std::ostream &o) : zip_streambuf(o.rdbuf()) {}

	zip_streambuf::zip_streambuf(std::streambuf *sb) : p_(std::make_unique<impl>(sb)) {}

	zip_streambuf::~zip_streambuf() = default;

	zip_streambuf &zip_streambuf::begin_entry(std::string name, std::string comment,
		stamp_t /*stamp*/, compression_t compression, level_t level) {
		p_->name = std::move(name);
		p_->comment = std::move(comment);
		p_->compression = compression;
		p_->level = level;
		p_->uncompressed_bytes = 0;
		p_->compressed_bytes = 0;
		p_->active = true;
		return *this;
	}

	zip_streambuf &zip_streambuf::end_entry() {
		p_->active = false;
		return *this;
	}

	void zip_streambuf::close(std::string_view /*archive_comment*/) {
		p_->closed = true;
	}

	bool zip_streambuf::is_open() const {
		return !p_->closed;
	}

	bool zip_streambuf::entry_active() const {
		return p_->active;
	}

	std::string_view zip_streambuf::entry_name() const {
		return p_->name;
	}

	compression_t zip_streambuf::entry_compression() const {
		return p_->compression;
	}

	level_t zip_streambuf::entry_level() const {
		return p_->level;
	}

	std::uint64_t zip_streambuf::entry_uncompressed_bytes() const {
		return p_->uncompressed_bytes;
	}

	std::uint64_t zip_streambuf::entry_compressed_bytes() const {
		return p_->compressed_bytes;
	}

	std::uint64_t zip_streambuf::current_offset() const {
		return p_->current_offset;
	}

	void zip_streambuf::set_offset(std::uint64_t offset) {
		p_->current_offset = offset;
	}

	std::streamsize zip_streambuf::xsputn(const char_type *s, std::streamsize n) {
		return p_->backing->sputn(s, n);
	}

	zip_streambuf::int_type zip_streambuf::overflow(int_type c) {
		if (c != traits_type::eof())
			return p_->backing->sputc(traits_type::to_char_type(c));
		return traits_type::eof();
	}

} // namespace zip_xx
