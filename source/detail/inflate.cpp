#include "inflate.h"
#include <stdexcept>

namespace zip_xx::detail {

	inflate_stream::inflate_stream() {
		if (::inflateInit2(&z_, -15) != Z_OK)
			throw std::runtime_error("inflateInit2 failed");
	}

	inflate_stream::~inflate_stream() {
		::inflateEnd(&z_);
	}

	void inflate_stream::reset() {
		::inflateEnd(&z_);
		z_ = {};
		if (::inflateInit2(&z_, -15) != Z_OK)
			throw std::runtime_error("inflateInit2 failed");
	}

	std::pair<std::size_t, std::size_t> inflate_stream::decompress(const std::byte *in,
		std::size_t in_len, std::byte *out, std::size_t out_len) {
		z_.next_in = reinterpret_cast<Bytef *>(const_cast<std::byte *>(in));
		z_.avail_in = static_cast<uInt>(in_len);
		z_.next_out = reinterpret_cast<Bytef *>(out);
		z_.avail_out = static_cast<uInt>(out_len);

		int ret = ::inflate(&z_, Z_NO_FLUSH);
		if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR)
			throw std::runtime_error("inflate stream error");

		return { in_len - z_.avail_in, out_len - z_.avail_out };
	}

} // namespace zip_xx::detail
