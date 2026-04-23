#include <zip-xx/zip-xx.h>
#include "detail/read_records.h"

namespace zip_xx {

	zip_archive::zip_archive(std::istream &is) : zip_archive(is.rdbuf()) {}

	zip_archive::zip_archive(std::streambuf *sb) : backing_(sb) {
		auto info = detail::find_eocd(backing_);
		backing_->pubseekpos(
			static_cast<std::streampos>(static_cast<std::streamoff>(info.central_dir_offset)),
			std::ios_base::in);
		entries_.reserve(info.entry_count);
		for (std::uint64_t i = 0; i < info.entry_count; ++i)
			entries_.push_back(detail::parse_central_dir_entry(backing_));
	}

	std::span<const zip_entry> zip_archive::entries() const {
		return std::span<const zip_entry>(entries_);
	}

	const zip_entry *zip_archive::find(std::string_view name) const {
		for (const auto &e : entries_)
			if (e.name == name)
				return &e;
		return nullptr;
	}

} // namespace zip_xx
