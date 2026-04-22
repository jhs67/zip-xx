#include "deflate.h"
#include <stdexcept>

namespace zip_xx::detail {

	// windowBits of -15 produces raw DEFLATE output (no zlib or gzip framing),
	// which is what the ZIP format requires.
	deflate_stream::deflate_stream(int zlib_level) {
		if (::deflateInit2(&z_, zlib_level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			throw std::runtime_error("deflateInit2 failed");
	}

	deflate_stream::~deflate_stream() {
		::deflateEnd(&z_);
	}

	std::size_t deflate_stream::compress(const std::byte *in, std::size_t len, std::streambuf *sb,
		bool finish) {
		constexpr uInt chunk = 32768;
		Bytef out[chunk];

		z_.next_in = reinterpret_cast<Bytef *>(const_cast<std::byte *>(in));
		z_.avail_in = static_cast<uInt>(len);

		int flush = finish ? Z_FINISH : Z_NO_FLUSH;
		std::size_t total = 0;

		do {
			z_.next_out = out;
			z_.avail_out = chunk;

			if (::deflate(&z_, flush) == Z_STREAM_ERROR)
				throw std::runtime_error("deflate stream error");

			auto produced = chunk - z_.avail_out;
			if (produced > 0) {
				sb->sputn(reinterpret_cast<const char *>(out), produced);
				total += produced;
			}
		} while (z_.avail_out == 0);

		return total;
	}

} // namespace zip_xx::detail
