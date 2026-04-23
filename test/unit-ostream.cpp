#include "zip-xx/zip-xx.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <zip.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

	// RAII wrapper for a libzip archive opened from a memory buffer.
	// The buffer string must outlive this object.
	struct ZipArchive {
		explicit ZipArchive(const std::string &buf) {
			zip_error_t err;
			zip_error_init(&err);
			auto *src = zip_source_buffer_create(buf.data(), buf.size(), 0, &err);
			REQUIRE(src != nullptr);
			handle = zip_open_from_source(src, ZIP_RDONLY, &err);
			if (!handle) {
				zip_source_free(src);
				FAIL("libzip failed to open archive");
			}
			zip_error_fini(&err);
		}

		~ZipArchive() {
			if (handle)
				zip_discard(handle);
		}

		ZipArchive(const ZipArchive &) = delete;
		ZipArchive &operator=(const ZipArchive &) = delete;

		zip_int64_t entry_count() const {
			return zip_get_num_entries(handle, 0);
		}

		zip_stat_t stat(zip_uint64_t index) const {
			zip_stat_t s;
			REQUIRE(zip_stat_index(handle, index, 0, &s) == 0);
			return s;
		}

		std::string read(zip_uint64_t index) const {
			auto s = stat(index);
			auto *file = zip_fopen_index(handle, index, 0);
			REQUIRE(file != nullptr);
			std::string content(static_cast<std::size_t>(s.size), '\0');
			std::size_t done = 0;
			while (done < content.size()) {
				auto n = zip_fread(file, content.data() + done, content.size() - done);
				REQUIRE(n > 0);
				done += static_cast<std::size_t>(n);
			}
			zip_fclose(file);
			return content;
		}

		std::string archive_comment() const {
			int len = 0;
			const char *c = zip_get_archive_comment(handle, &len, 0);
			if (!c || len == 0)
				return {};
			return { c, static_cast<std::size_t>(len) };
		}

		std::string entry_comment(zip_uint64_t index) const {
			zip_uint32_t len = 0;
			const char *c = zip_file_get_comment(handle, index, &len, 0);
			if (!c || len == 0)
				return {};
			return { c, static_cast<std::size_t>(len) };
		}

		zip_t *handle = nullptr;
	};

	// Backing streambuf that forwards writes but rejects all seek operations,
	// simulating a non-seekable stream (pipe, socket, etc.).
	class non_seekable_buf : public std::streambuf {
	  public:
		explicit non_seekable_buf(std::streambuf *sink) : sink_(sink) {}

	  protected:
		std::streamsize xsputn(const char_type *s, std::streamsize n) override {
			return sink_->sputn(s, n);
		}

		int_type overflow(int_type c) override {
			if (c != traits_type::eof())
				return sink_->sputc(traits_type::to_char_type(c));
			return traits_type::eof();
		}

		// seekpos/seekoff deliberately not overridden — base returns failure

	  private:
		std::streambuf *sink_;
	};

	// Read a 4-byte little-endian uint32 from a string at the given offset.
	std::uint32_t read_le32(const std::string &data, std::size_t offset) {
		return static_cast<std::uint8_t>(data[offset]) |
			(static_cast<std::uint8_t>(data[offset + 1]) << 8) |
			(static_cast<std::uint8_t>(data[offset + 2]) << 16) |
			(static_cast<std::uint8_t>(data[offset + 3]) << 24);
	}

} // namespace

// ---------------------------------------------------------------------------
// Round-trip tests
// ---------------------------------------------------------------------------

TEST_CASE("single DEFLATE entry round-trip") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("hello.txt");
	zos << "Hello, World!";
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	auto s = arch.stat(0);
	REQUIRE(std::string(s.name) == "hello.txt");
	REQUIRE(s.comp_method == ZIP_CM_DEFLATE);
	REQUIRE(arch.read(0) == "Hello, World!");
}

TEST_CASE("multiple entries") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("a.txt");
	zos << "alpha";
	zos.end_entry();
	zos.begin_entry("b.txt");
	zos << "bravo";
	zos.end_entry();
	zos.begin_entry("c.txt");
	zos << "charlie";
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 3);
	REQUIRE(arch.read(0) == "alpha");
	REQUIRE(arch.read(1) == "bravo");
	REQUIRE(arch.read(2) == "charlie");
}

TEST_CASE("STORED entry round-trip") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("stored.txt", {}, zip_xx::stamp_t{}, zip_xx::compression_t::store);
	zos << "uncompressed content";
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	auto s = arch.stat(0);
	REQUIRE(s.comp_method == ZIP_CM_STORE);
	REQUIRE(s.size == s.comp_size);
	REQUIRE(arch.read(0) == "uncompressed content");
}

TEST_CASE("empty entry") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("empty.txt");
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	REQUIRE(arch.stat(0).size == 0);
	REQUIRE(arch.read(0).empty());
}

TEST_CASE("large entry forces multiple deflate chunks") {
	// Content larger than deflate_stream's 32 KiB output buffer
	std::string content(100'000, 'x');

	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("large.txt");
	zos << content;
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.read(0) == content);
}

TEST_CASE("unicode filename") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("日本語/ファイル.txt");
	zos << "unicode";
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	REQUIRE(std::string(arch.stat(0).name) == "日本語/ファイル.txt");
	REQUIRE(arch.read(0) == "unicode");
}

TEST_CASE("entry comment") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("file.txt", "this is the entry comment");
	zos << "data";
	zos.end_entry();
	zos.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_comment(0) == "this is the entry comment");
}

TEST_CASE("archive comment") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("file.txt");
	zos << "data";
	zos.end_entry();
	zos.close("archive-level comment");

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.archive_comment() == "archive-level comment");
}

// ---------------------------------------------------------------------------
// Compression levels
// ---------------------------------------------------------------------------

TEST_CASE("all compression levels produce valid output") {
	using zip_xx::compression_t;
	using zip_xx::level_t;

	// Compressible content: repeated pattern
	std::string content(10'000, '\0');
	for (std::size_t i = 0; i < content.size(); ++i)
		content[i] = static_cast<char>(i % 64);

	for (auto level :
		{ level_t::minimal, level_t::low, level_t::normal, level_t::high, level_t::maximum }) {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.begin_entry("data.bin", {}, zip_xx::stamp_t{}, compression_t::deflate, level);
		zos.write(content.data(), static_cast<std::streamsize>(content.size()));
		zos.end_entry();
		zos.close();

		auto data = ss.str();
		ZipArchive arch(data);
		REQUIRE(arch.entry_count() == 1);
		REQUIRE(arch.read(0) == content);
	}
}

TEST_CASE("higher compression levels reduce size for compressible data") {
	std::string content(50'000, 'A'); // highly compressible

	auto compressed_size = [&](zip_xx::level_t level) {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.begin_entry("data.bin", {}, zip_xx::stamp_t{}, zip_xx::compression_t::deflate, level);
		zos.write(content.data(), static_cast<std::streamsize>(content.size()));
		zos.end_entry();
		zos.close();
		return zos.entry_compressed_bytes();
	};

	auto minimal = compressed_size(zip_xx::level_t::minimal);
	auto maximum = compressed_size(zip_xx::level_t::maximum);
	REQUIRE(maximum <= minimal);
}

// ---------------------------------------------------------------------------
// Timestamps
// ---------------------------------------------------------------------------

TEST_CASE("mtime is preserved in extended timestamp extra field") {
	// Use a fixed Unix timestamp (2023-11-14 22:13:20 UTC)
	constexpr std::uint32_t expected_mtime = 1700000000;
	auto stamp = std::chrono::system_clock::from_time_t(expected_mtime);

	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("file.txt", {}, stamp);
	zos << "data";
	zos.end_entry();
	zos.close();

	auto data = ss.str();

	// Verify the archive is readable
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);

	// Read the unix mtime directly from the extended timestamp extra field in
	// the local file header. For "file.txt" (8 chars) the layout is:
	//   30 fixed + 8 name + 20 ZIP64 extra + 4 ext-stamp header + 1 flags = 63
	//   mtime occupies bytes 63-66.
	REQUIRE(data.size() > 67);
	REQUIRE(read_le32(data, 63) == expected_mtime);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST_CASE("error handling") {
	SECTION("begin_entry while entry is active throws") {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.begin_entry("a.txt");
		REQUIRE_THROWS_AS(zos.begin_entry("b.txt"), std::logic_error);
	}

	SECTION("end_entry with no active entry throws") {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		REQUIRE_THROWS_AS(zos.end_entry(), std::logic_error);
	}

	SECTION("close twice throws") {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.close();
		REQUIRE_THROWS_AS(zos.close(), std::logic_error);
	}

	SECTION("begin_entry on closed archive throws") {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.close();
		REQUIRE_THROWS_AS(zos.begin_entry("x.txt"), std::logic_error);
	}
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

TEST_CASE("state queries") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);

	REQUIRE(zos.is_open());
	REQUIRE_FALSE(zos.entry_active());

	zos.begin_entry("file.txt", "comment", zip_xx::stamp_t{}, zip_xx::compression_t::store,
		zip_xx::level_t::high);

	REQUIRE(zos.entry_active());
	REQUIRE(zos.entry_name() == "file.txt");
	REQUIRE(zos.entry_compression() == zip_xx::compression_t::store);
	REQUIRE(zos.entry_level() == zip_xx::level_t::high);

	zos << "abc";
	REQUIRE(zos.entry_uncompressed_bytes() == 3);

	zos.end_entry();
	REQUIRE_FALSE(zos.entry_active());
	// Most recent entry info is retained after end_entry
	REQUIRE(zos.entry_name() == "file.txt");

	zos.close();
	REQUIRE_FALSE(zos.is_open());
}

// ---------------------------------------------------------------------------
// set_offset (self-extracting archive simulation)
// ---------------------------------------------------------------------------

TEST_CASE("set_offset shifts all central directory offsets") {
	// Write a stub prefix, then a ZIP whose offsets account for the prefix.
	std::stringstream ss;
	const std::string prefix(256, '\x90'); // 256-byte NOP sled stand-in
	ss.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));

	zip_xx::zip_ostream zos(ss);
	zos.set_offset(prefix.size());
	zos.begin_entry("file.txt");
	zos << "self-extracting content";
	zos.end_entry();
	zos.close();

	// libzip opens the full buffer; offsets recorded in the central directory
	// include the prefix, so it can seek correctly to find each local header.
	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	REQUIRE(arch.read(0) == "self-extracting content");
}

// ---------------------------------------------------------------------------
// Non-seekable backing stream
// ---------------------------------------------------------------------------

TEST_CASE("non-seekable backing stream produces valid archive") {
	std::stringstream sink;
	non_seekable_buf nsb(sink.rdbuf());

	zip_xx::zip_ostream zos(&nsb);
	zos.begin_entry("file.txt");
	zos << "non-seekable output";
	zos.end_entry();
	zos.close();

	auto data = sink.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	REQUIRE(arch.read(0) == "non-seekable output");
}

// ---------------------------------------------------------------------------
// ZIP64 structural invariants
// ---------------------------------------------------------------------------

TEST_CASE("standard EOCD carries ZIP64 sentinels") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("f.txt");
	zos << "x";
	zos.end_entry();
	zos.close();

	auto data = ss.str();

	// The standard EOCD is the last 22 bytes (no archive comment).
	REQUIRE(data.size() >= 22);
	auto eocd = data.size() - 22;

	// Verify EOCD signature: PK\x05\x06
	REQUIRE(read_le32(data, eocd) == 0x06054b50u);

	// Central directory size and offset must both be 0xFFFFFFFF (ZIP64 sentinels).
	REQUIRE(read_le32(data, eocd + 12) == 0xFFFFFFFFu);
	REQUIRE(read_le32(data, eocd + 16) == 0xFFFFFFFFu);

	// The ZIP64 EOCD locator immediately precedes the EOCD (20 bytes).
	REQUIRE(data.size() >= 42);
	auto locator = data.size() - 42;
	REQUIRE(read_le32(data, locator) == 0x07064b50u);

	// The ZIP64 EOCD precedes the locator (56 bytes).
	REQUIRE(data.size() >= 98);
	auto zip64_eocd = data.size() - 98;
	REQUIRE(read_le32(data, zip64_eocd) == 0x06064b50u);

	// Entry count in ZIP64 EOCD: 8-byte field at offset 32 — should be 1.
	std::uint64_t z64_count = 0;
	for (int i = 7; i >= 0; --i)
		z64_count = (z64_count << 8) | static_cast<std::uint8_t>(data[zip64_eocd + 32 + i]);
	REQUIRE(z64_count == 1);
}

// ---------------------------------------------------------------------------
// zip_streambuf used directly
// ---------------------------------------------------------------------------

TEST_CASE("zip_streambuf used directly without zip_ostream") {
	std::stringstream ss;
	zip_xx::zip_streambuf zsb(ss);
	std::ostream os(&zsb);

	zsb.begin_entry("direct.txt");
	os << "written via streambuf";
	zsb.end_entry();
	zsb.close();

	auto data = ss.str();
	ZipArchive arch(data);
	REQUIRE(arch.entry_count() == 1);
	REQUIRE(std::string(arch.stat(0).name) == "direct.txt");
	REQUIRE(arch.read(0) == "written via streambuf");
}
