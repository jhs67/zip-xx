#include <zip-xx/zip-xx.h>

namespace zip_xx {

	zip_istream::zip_istream() : std::istream(nullptr) {}

	zip_istream::zip_istream(zip_archive &arc, const zip_entry &entry) : std::istream(nullptr) {
		buf_ = std::make_unique<zip_istreambuf>(arc, entry);
		std::istream::rdbuf(buf_.get());
	}

	zip_istream::~zip_istream() = default;

	void zip_istream::open(zip_archive &arc, const zip_entry &entry) {
		buf_ = std::make_unique<zip_istreambuf>(arc, entry);
		std::istream::rdbuf(buf_.get());
		clear();
	}

	zip_istreambuf *zip_istream::rdbuf() const {
		return static_cast<zip_istreambuf *>(std::istream::rdbuf());
	}

} // namespace zip_xx
