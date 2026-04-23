#pragma once
#include <cstddef>
#include <utility>
#include <zlib.h>

namespace zip_xx::detail {

	class inflate_stream {
	  public:
		inflate_stream();
		~inflate_stream();

		inflate_stream(const inflate_stream &) = delete;
		inflate_stream &operator=(const inflate_stream &) = delete;

		// Reset to initial state for backward seeking: inflateEnd + inflateInit2.
		void reset();

		// Decompress up to in_len compressed bytes from in, writing decompressed
		// output into out (up to out_len bytes). Returns {bytes consumed from in,
		// bytes written to out}.
		std::pair<std::size_t, std::size_t> decompress(const std::byte *in, std::size_t in_len,
			std::byte *out, std::size_t out_len);

	  private:
		z_stream z_{};
	};

} // namespace zip_xx::detail
