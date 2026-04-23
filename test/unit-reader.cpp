#include "zip-xx/zip-xx.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

	// Write a single-entry archive and return its bytes.
	std::string single_entry(std::string_view name, std::string_view content,
		zip_xx::compression_t comp = zip_xx::compression_t::deflate, std::string_view comment = {},
		zip_xx::stamp_t stamp = {}) {
		std::stringstream ss;
		zip_xx::zip_ostream zos(ss);
		zos.begin_entry(std::string(name), std::string(comment), stamp, comp);
		zos.write(content.data(), static_cast<std::streamsize>(content.size()));
		zos.end_entry();
		zos.close();
		return ss.str();
	}

	// Drain an istream to a string.
	std::string slurp(std::istream &is) {
		return { std::istreambuf_iterator<char>(is), {} };
	}

	// CRC-32 (IEEE 802.3 polynomial) for the legacy-archive builder.
	std::uint32_t crc32_of(std::string_view data) {
		std::uint32_t crc = 0xFFFFFFFFu;
		for (auto ch : data) {
			crc ^= static_cast<std::uint8_t>(ch);
			for (int k = 0; k < 8; ++k)
				crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
		}
		return crc ^ 0xFFFFFFFFu;
	}

	// Build a minimal non-ZIP64 archive (single STORED entry, no extra fields).
	std::string make_legacy_archive(std::string_view filename, std::string_view content) {
		auto u16 = [](std::string &s, std::uint16_t v) {
			s += static_cast<char>(v & 0xFF);
			s += static_cast<char>((v >> 8) & 0xFF);
		};
		auto u32 = [](std::string &s, std::uint32_t v) {
			s += static_cast<char>(v & 0xFF);
			s += static_cast<char>((v >> 8) & 0xFF);
			s += static_cast<char>((v >> 16) & 0xFF);
			s += static_cast<char>((v >> 24) & 0xFF);
		};

		auto crc = crc32_of(content);
		auto flen = static_cast<std::uint16_t>(filename.size());
		auto size = static_cast<std::uint32_t>(content.size());

		std::string out;

		// Local file header (30 bytes + filename)
		auto local_off = static_cast<std::uint32_t>(out.size());
		u32(out, 0x04034b50); // signature
		u16(out, 20);		  // version needed
		u16(out, 0);		  // GP flags
		u16(out, 0);		  // compression: stored
		u16(out, 0);		  // mod time
		u16(out, 0);		  // mod date
		u32(out, crc);
		u32(out, size); // compressed size
		u32(out, size); // uncompressed size
		u16(out, flen);
		u16(out, 0); // extra length
		out += filename;

		out += content;

		// Central directory (46 bytes + filename)
		auto cd_off = static_cast<std::uint32_t>(out.size());
		u32(out, 0x02014b50); // signature
		u16(out, 20);		  // version made by
		u16(out, 20);		  // version needed
		u16(out, 0);		  // GP flags
		u16(out, 0);		  // compression: stored
		u16(out, 0);		  // mod time
		u16(out, 0);		  // mod date
		u32(out, crc);
		u32(out, size); // compressed size
		u32(out, size); // uncompressed size
		u16(out, flen);
		u16(out, 0); // extra length
		u16(out, 0); // comment length
		u16(out, 0); // disk number start
		u16(out, 0); // internal attrs
		u32(out, 0); // external attrs
		u32(out, local_off);
		out += filename;

		auto cd_size = static_cast<std::uint32_t>(out.size() - cd_off);

		// Standard EOCD (22 bytes, no ZIP64 records)
		u32(out, 0x06054b50); // signature
		u16(out, 0);		  // disk number
		u16(out, 0);		  // disk with CD
		u16(out, 1);		  // entries on this disk
		u16(out, 1);		  // total entries
		u32(out, cd_size);
		u32(out, cd_off);
		u16(out, 0); // comment length

		return out;
	}

} // namespace

// ---------------------------------------------------------------------------
// Round-trip tests
// ---------------------------------------------------------------------------

TEST_CASE("single STORED entry round-trip") {
	auto bytes = single_entry("stored.txt", "uncompressed content", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries().size() == 1);
	REQUIRE(arc.entries()[0].name == "stored.txt");
	REQUIRE(arc.entries()[0].compression == zip_xx::compression_t::store);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == "uncompressed content");
}

TEST_CASE("reader: single DEFLATE entry round-trip") {
	auto bytes = single_entry("deflate.txt", "Hello, compressed world!");
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries().size() == 1);
	REQUIRE(arc.entries()[0].name == "deflate.txt");
	REQUIRE(arc.entries()[0].compression == zip_xx::compression_t::deflate);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == "Hello, compressed world!");
}

TEST_CASE("reader: multiple entries") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("alpha.txt");
	zos << "first";
	zos.end_entry();
	zos.begin_entry("beta.txt");
	zos << "second";
	zos.end_entry();
	zos.begin_entry("gamma.txt");
	zos << "third";
	zos.end_entry();
	zos.close();

	std::istringstream is(ss.str());
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries().size() == 3);
	REQUIRE(arc.entries()[0].name == "alpha.txt");
	REQUIRE(arc.entries()[1].name == "beta.txt");
	REQUIRE(arc.entries()[2].name == "gamma.txt");

	for (std::size_t i = 0; i < 3; ++i) {
		zip_xx::zip_istream zis(arc, arc.entries()[i]);
		std::string expected[] = { "first", "second", "third" };
		REQUIRE(slurp(zis) == expected[i]);
	}
}

TEST_CASE("find hit and miss") {
	auto bytes = single_entry("needle.txt", "found it");
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.find("needle.txt") != nullptr);
	REQUIRE(arc.find("needle.txt")->name == "needle.txt");
	REQUIRE(arc.find("haystack.txt") == nullptr);
}

TEST_CASE("large entry forces multiple underflow calls") {
	// 100 KiB, larger than the 32 KiB get buffer
	std::string content(100'000, '\0');
	for (std::size_t i = 0; i < content.size(); ++i)
		content[i] = static_cast<char>('a' + (i % 26));

	auto bytes = single_entry("large.txt", content);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == content);
}

TEST_CASE("reader: empty entry") {
	auto bytes = single_entry("empty.txt", "");
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries()[0].uncompressed_size == 0);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(zis.get() == std::char_traits<char>::eof());
}

TEST_CASE("entry with comment") {
	auto bytes =
		single_entry("file.txt", "data", zip_xx::compression_t::store, "this is the entry comment");
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries()[0].comment == "this is the entry comment");
}

TEST_CASE("timestamp") {
	constexpr std::uint32_t unix_mtime = 1700000000; // 2023-11-14 22:13:20 UTC
	auto stamp = std::chrono::system_clock::from_time_t(unix_mtime);

	auto bytes = single_entry("ts.txt", "data", zip_xx::compression_t::store, {}, stamp);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	using namespace std::chrono;
	auto diff = duration_cast<seconds>(arc.entries()[0].timestamp - stamp).count();
	REQUIRE(std::abs(diff) <= 1);
}

// ---------------------------------------------------------------------------
// Interface tests
// ---------------------------------------------------------------------------

TEST_CASE("read via zip_istreambuf directly") {
	auto bytes = single_entry("direct.txt", "streambuf content", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istreambuf buf(arc, arc.entries()[0]);
	std::string result;
	char tmp[64];
	std::streamsize n;
	while ((n = buf.sgetn(tmp, sizeof(tmp))) > 0)
		result.append(tmp, static_cast<std::size_t>(n));

	REQUIRE(result == "streambuf content");
}

TEST_CASE("read via zip_istream") {
	auto bytes = single_entry("stream.txt", "42 hello", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	int n;
	std::string word;
	zis >> n >> word;
	REQUIRE(n == 42);
	REQUIRE(word == "hello");
}

TEST_CASE("open on existing zip_istream") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("a.txt", {}, {}, zip_xx::compression_t::store);
	zos << "entry-a";
	zos.end_entry();
	zos.begin_entry("b.txt", {}, {}, zip_xx::compression_t::store);
	zos << "entry-b";
	zos.end_entry();
	zos.close();

	std::istringstream is(ss.str());
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == "entry-a");

	zis.open(arc, arc.entries()[1]);
	REQUIRE(slurp(zis) == "entry-b");
}

TEST_CASE("multiple entries open simultaneously") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	zos.begin_entry("a.txt", {}, {}, zip_xx::compression_t::store);
	zos << "AAABBBCCC";
	zos.end_entry();
	zos.begin_entry("b.txt", {}, {}, zip_xx::compression_t::store);
	zos << "XXXYYYZZZ";
	zos.end_entry();
	zos.close();

	std::istringstream is(ss.str());
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream sa(arc, arc.entries()[0]);
	zip_xx::zip_istream sb(arc, arc.entries()[1]);

	char a[3], b[3];

	sa.read(a, 3);
	sb.read(b, 3);
	REQUIRE(std::string(a, 3) == "AAA");
	REQUIRE(std::string(b, 3) == "XXX");

	sa.read(a, 3);
	sb.read(b, 3);
	REQUIRE(std::string(a, 3) == "BBB");
	REQUIRE(std::string(b, 3) == "YYY");

	sa.read(a, 3);
	sb.read(b, 3);
	REQUIRE(std::string(a, 3) == "CCC");
	REQUIRE(std::string(b, 3) == "ZZZ");
}

// ---------------------------------------------------------------------------
// Seek tests
// ---------------------------------------------------------------------------

TEST_CASE("seek forward STORED") {
	auto bytes = single_entry("s.txt", "ABCDEFGHIJKLMNOPQRST", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	zis.seekg(5);
	char buf[5];
	zis.read(buf, 5);
	REQUIRE(std::string(buf, 5) == "FGHIJ");
}

TEST_CASE("seek backward STORED") {
	auto bytes = single_entry("s.txt", "ABCDEFGHIJKLMNOPQRST", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);

	// Read 10 bytes to advance the position.
	char discard[10];
	zis.read(discard, 10);

	zis.seekg(2);
	char buf[4];
	zis.read(buf, 4);
	REQUIRE(std::string(buf, 4) == "CDEF");
}

TEST_CASE("seek forward DEFLATE") {
	// 2000-byte repeating alphabet so position is predictable.
	std::string content(2000, '\0');
	for (std::size_t i = 0; i < content.size(); ++i)
		content[i] = static_cast<char>('a' + (i % 26));

	auto bytes = single_entry("d.txt", content);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	zis.seekg(500);
	char buf[5];
	zis.read(buf, 5);
	REQUIRE(std::string(buf, 5) == content.substr(500, 5));
}

TEST_CASE("seek backward DEFLATE") {
	std::string content(2000, '\0');
	for (std::size_t i = 0; i < content.size(); ++i)
		content[i] = static_cast<char>('a' + (i % 26));

	auto bytes = single_entry("d.txt", content);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);

	// Read past position 600 to advance the inflate state.
	char discard[600];
	zis.read(discard, 600);

	// Seek backward to 100, triggering reset + discard.
	zis.seekg(100);
	char buf[5];
	zis.read(buf, 5);
	REQUIRE(std::string(buf, 5) == content.substr(100, 5));
}

TEST_CASE("seek to 0 DEFLATE") {
	std::string content(1000, '\0');
	for (std::size_t i = 0; i < content.size(); ++i)
		content[i] = static_cast<char>('a' + (i % 26));

	auto bytes = single_entry("d.txt", content);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == content);

	zis.seekg(0);
	REQUIRE(slurp(zis) == content);
}

TEST_CASE("tellg") {
	auto bytes = single_entry("t.txt", "ABCDEFGHIJ", zip_xx::compression_t::store);
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(zis.tellg() == 0);

	char buf[3];
	zis.read(buf, 3);
	REQUIRE(zis.tellg() == 3);

	zis.seekg(7);
	REQUIRE(zis.tellg() == 7);

	zis.get(); // reads 'H'
	REQUIRE(zis.tellg() == 8);
}

// ---------------------------------------------------------------------------
// Non-ZIP64 legacy archive
// ---------------------------------------------------------------------------

TEST_CASE("non-ZIP64 legacy archive") {
	auto bytes = make_legacy_archive("legacy.txt", "classic zip content");
	std::istringstream is(bytes);
	zip_xx::zip_archive arc(is);

	REQUIRE(arc.entries().size() == 1);
	REQUIRE(arc.entries()[0].name == "legacy.txt");
	REQUIRE(arc.entries()[0].uncompressed_size == 19);
	REQUIRE(arc.entries()[0].compression == zip_xx::compression_t::store);

	zip_xx::zip_istream zis(arc, arc.entries()[0]);
	REQUIRE(slurp(zis) == "classic zip content");
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------

TEST_CASE("malformed signature throws") {
	std::istringstream is("this is not a zip file at all, no zip here");
	REQUIRE_THROWS_AS(zip_xx::zip_archive(is), std::runtime_error);
}

TEST_CASE("truncated archive throws") {
	std::istringstream is(std::string(10, '\x00'));
	REQUIRE_THROWS_AS(zip_xx::zip_archive(is), std::runtime_error);
}

// ---------------------------------------------------------------------------
// ZIP64 entry count (>65535 entries)
// ---------------------------------------------------------------------------

TEST_CASE("archive with many entries") {
	std::stringstream ss;
	zip_xx::zip_ostream zos(ss);
	for (int i = 0; i < 70'000; ++i) {
		zos.begin_entry(std::to_string(i), {}, {}, zip_xx::compression_t::store);
		zos.end_entry();
	}
	zos.close();

	std::istringstream is(ss.str());
	zip_xx::zip_archive arc(is);
	REQUIRE(arc.entries().size() == 70'000);
	REQUIRE(arc.entries()[0].name == "0");
	REQUIRE(arc.entries()[69'999].name == "69999");
}
