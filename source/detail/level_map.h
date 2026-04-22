#pragma once
#include "zip-xx/zip-xx.h"
#include <zlib.h>

namespace zip_xx::detail {

	inline int to_zlib_level(level_t level) {
		switch (level) {
			case level_t::minimal:
				return 1;
			case level_t::low:
				return 3;
			case level_t::normal:
				return Z_DEFAULT_COMPRESSION;
			case level_t::high:
				return 8;
			case level_t::maximum:
				return 9;
		}
		return Z_DEFAULT_COMPRESSION;
	}

} // namespace zip_xx::detail
