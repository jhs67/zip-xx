#ifndef H_ZIP_XX_ZIP_XX
#define H_ZIP_XX_ZIP_XX

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

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

	class zip_streambuf : public std::streambuf {
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

		struct zip_ostream_base {
			zip_streambuf buf_;
			explicit zip_ostream_base(std::streambuf *sb) : buf_(sb) {}
		};

	} // namespace detail

	class zip_ostream : private detail::zip_ostream_base, public std::ostream {
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

} // namespace zip_xx

#endif // H_ZIP_XX_ZIP_XX
