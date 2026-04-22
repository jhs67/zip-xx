#pragma once
#include <cstddef>
#include <cstdint>
#include <zlib.h>

namespace zip_xx::detail {

	class crc_accumulator {
	  public:
		void update(const std::byte *data, std::size_t len) {
			crc_ = static_cast<std::uint32_t>(
				::crc32(crc_, reinterpret_cast<const Bytef *>(data), static_cast<uInt>(len)));
		}

		void reset() {
			crc_ = 0;
		}

		std::uint32_t value() const {
			return crc_;
		}

	  private:
		std::uint32_t crc_ = 0;
	};

} // namespace zip_xx::detail
