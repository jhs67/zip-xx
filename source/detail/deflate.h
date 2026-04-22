#pragma once
#include <cstddef>
#include <streambuf>
#include <zlib.h>

namespace zip_xx::detail {

	class deflate_stream {
	  public:
		explicit deflate_stream(int zlib_level);
		~deflate_stream();

		deflate_stream(const deflate_stream &) = delete;
		deflate_stream &operator=(const deflate_stream &) = delete;

		// Feed uncompressed bytes and write compressed output to sb.
		// Returns the number of compressed bytes written to sb.
		// Call with finish=true on the final call to flush and close the stream.
		std::size_t compress(const std::byte *in, std::size_t len, std::streambuf *sb,
			bool finish = false);

	  private:
		z_stream z_{};
	};

} // namespace zip_xx::detail
