#ifndef H_ZIP_XX_ZIP_XX
#define H_ZIP_XX_ZIP_XX

#include <iostream>
#include <chrono>

namespace zip_xx {

	struct zip_ostream : public std::ostream {
		using stamp_t = std::chrono::time_point<std::chrono::system_clock>;

		enum struct compression_t {
			store,
			deflate,
		};

		zip_ostream(std::ostream &o);

		void begin_entry(std::string name, std::string comment = {},
			stamp_t stamp = std::chrono::system_clock::now(),
			compression_t compression = compression_t::deflate);

		void end_entry(std::string_view comment = {});

		void close();
	};

} // namespace zip_xx

#endif // H_ZIP_XX_ZIP_XX
