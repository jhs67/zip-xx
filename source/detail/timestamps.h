#pragma once
#include <chrono>
#include <cstdint>
#include <utility>

namespace zip_xx::detail {

	using stamp_t = std::chrono::time_point<std::chrono::system_clock>;

	// Returns {dos_time, dos_date}.
	// DOS date: bits 15-9 = year-1980, bits 8-5 = month, bits 4-0 = day
	// DOS time: bits 15-11 = hour, bits 10-5 = minute, bits 4-0 = second/2
	inline std::pair<std::uint16_t, std::uint16_t> to_dos_datetime(stamp_t tp) {
		using namespace std::chrono;
		auto dp = floor<days>(tp);
		year_month_day ymd{ dp };
		hh_mm_ss hms{ floor<seconds>(tp) - dp };

		auto y = static_cast<int>(ymd.year()) - 1980;
		auto m = static_cast<unsigned>(ymd.month());
		auto d = static_cast<unsigned>(ymd.day());
		auto h = static_cast<unsigned>(hms.hours().count());
		auto min = static_cast<unsigned>(hms.minutes().count());
		auto sec = static_cast<unsigned>(hms.seconds().count());

		auto dos_date = static_cast<std::uint16_t>((y << 9) | (m << 5) | d);
		auto dos_time = static_cast<std::uint16_t>((h << 11) | (min << 5) | (sec / 2));
		return { dos_time, dos_date };
	}

	// Returns seconds since the Unix epoch, truncated to uint32_t.
	inline std::uint32_t to_unix_time(stamp_t tp) {
		using namespace std::chrono;
		return static_cast<std::uint32_t>(duration_cast<seconds>(tp.time_since_epoch()).count());
	}

} // namespace zip_xx::detail
