#ifndef H_ZIP_XX_ZIP_XX
#define H_ZIP_XX_ZIP_XX

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "zip_xx_export.h"

namespace zip_xx {

	using stamp_t = std::chrono::time_point<std::chrono::system_clock>;

	enum class compression_t {
		store,
		deflate,
	};

	enum class level_t {
		minimal,
		low,
		normal,
		high,
		maximum,
	};

	class ZIP_XX_EXPORT zip_streambuf : public std::streambuf {
	  public:
		explicit zip_streambuf(std::ostream &o);
		explicit zip_streambuf(std::streambuf *sb);
		~zip_streambuf();

		zip_streambuf(const zip_streambuf &) = delete;
		zip_streambuf &operator=(const zip_streambuf &) = delete;

		zip_streambuf &begin_entry(std::string name, std::string comment = {},
			stamp_t stamp = std::chrono::system_clock::now(),
			compression_t compression = compression_t::deflate, level_t level = level_t::normal);

		zip_streambuf &end_entry();
		void close(std::string_view archive_comment = {});

		bool is_open() const;
		bool entry_active() const;

		std::string_view entry_name() const;
		compression_t entry_compression() const;
		level_t entry_level() const;
		std::uint64_t entry_uncompressed_bytes() const;
		std::uint64_t entry_compressed_bytes() const;

		std::uint64_t current_offset() const;
		void set_offset(std::uint64_t);

	  protected:
		std::streamsize xsputn(const char_type *s, std::streamsize n) override;
		int_type overflow(int_type c) override;
		int sync() override;

	  private:
		void flush_put_buffer();
		struct impl;
		std::unique_ptr<impl> p_;
	};

	namespace detail {

		struct ZIP_XX_EXPORT zip_ostream_base {
			zip_streambuf buf_;
			explicit zip_ostream_base(std::streambuf *sb) : buf_(sb) {}
		};

	} // namespace detail

	class ZIP_XX_EXPORT zip_ostream : private detail::zip_ostream_base, public std::ostream {
	  public:
		explicit zip_ostream(std::ostream &o);
		explicit zip_ostream(std::streambuf *sb);

		zip_ostream &begin_entry(std::string name, std::string comment = {},
			stamp_t stamp = std::chrono::system_clock::now(),
			compression_t compression = compression_t::deflate, level_t level = level_t::normal);

		zip_ostream &end_entry();
		void close(std::string_view archive_comment = {});

		bool is_open() const;
		bool entry_active() const;

		std::string_view entry_name() const;
		compression_t entry_compression() const;
		level_t entry_level() const;
		std::uint64_t entry_uncompressed_bytes() const;
		std::uint64_t entry_compressed_bytes() const;

		std::uint64_t current_offset() const;
		void set_offset(std::uint64_t);

		zip_streambuf *rdbuf() const;
	};

	struct ZIP_XX_EXPORT zip_entry {
		std::string name;
		std::uint64_t uncompressed_size = 0;
		std::uint64_t compressed_size = 0;
		compression_t compression = compression_t::store;
		stamp_t timestamp = {};
		std::string comment;
		std::uint64_t local_header_offset = 0; // set by zip_archive; do not modify
	};

	class zip_istreambuf;

	class ZIP_XX_EXPORT zip_archive {
	  public:
		explicit zip_archive(std::istream &is);
		explicit zip_archive(std::streambuf *sb);

		zip_archive(const zip_archive &) = delete;
		zip_archive &operator=(const zip_archive &) = delete;

		std::span<const zip_entry> entries() const;
		const zip_entry *find(std::string_view name) const;

	  private:
		std::streambuf *backing_;
		std::vector<zip_entry> entries_;
		friend class zip_istreambuf;
	};

	class ZIP_XX_EXPORT zip_istreambuf : public std::streambuf {
	  public:
		zip_istreambuf(zip_archive &arc, const zip_entry &entry);
		~zip_istreambuf();

		zip_istreambuf(const zip_istreambuf &) = delete;
		zip_istreambuf &operator=(const zip_istreambuf &) = delete;

	  protected:
		int_type underflow() override;
		std::streamsize xsgetn(char_type *s, std::streamsize n) override;
		pos_type seekoff(off_type off, std::ios_base::seekdir way,
			std::ios_base::openmode which) override;
		pos_type seekpos(pos_type sp, std::ios_base::openmode which) override;

	  private:
		void discard(std::uint64_t n);

		struct impl;
		std::unique_ptr<impl> p_;
	};

	class ZIP_XX_EXPORT zip_istream : public std::istream {
	  public:
		zip_istream();
		zip_istream(zip_archive &arc, const zip_entry &entry);
		~zip_istream();

		void open(zip_archive &arc, const zip_entry &entry);

		zip_istreambuf *rdbuf() const;

	  private:
		std::unique_ptr<zip_istreambuf> buf_;
	};

} // namespace zip_xx

#endif // H_ZIP_XX_ZIP_XX
