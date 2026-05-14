#include <zip-xx/zip-xx.h>
#include "detail/inflate.h"
#include "detail/read_records.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

namespace zip_xx {

	struct zip_istreambuf::impl {
		zip_archive &archive;
		std::streambuf *backing;

		std::uint64_t data_start;
		std::uint64_t compressed_total;
		std::uint64_t uncompressed_total;
		compression_t compression;

		std::uint64_t compressed_fed = 0;
		std::uint64_t decompressed_pos = 0;

		std::optional<detail::inflate_stream> inflater;

		std::array<char, 32 * 1024> get_buf;
		std::array<char, 32 * 1024> compressed_buf;
	};

	zip_istreambuf::zip_istreambuf(zip_archive &arc, const zip_entry &entry) {
		auto data_start =
			detail::read_local_header_data_offset(arc.backing_, entry.local_header_offset);
		// new impl{} in-place avoids the move constructor that make_unique would require,
		// but impl is non-movable because optional<inflate_stream> is non-movable.
		p_.reset(new impl{
			.archive = arc,
			.backing = arc.backing_,
			.data_start = data_start,
			.compressed_total = entry.compressed_size,
			.uncompressed_total = entry.uncompressed_size,
			.compression = entry.compression,
		});
		if (entry.compression == compression_t::deflate)
			p_->inflater.emplace();
		setg(p_->get_buf.data(), p_->get_buf.data(), p_->get_buf.data());
	}

	zip_istreambuf::~zip_istreambuf() = default;

	zip_istreambuf::int_type zip_istreambuf::underflow() {
		if (p_->decompressed_pos >= p_->uncompressed_total)
			return traits_type::eof();

		if (p_->compression == compression_t::store) {
			p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
										p_->data_start + p_->decompressed_pos)),
				std::ios_base::in);

			auto to_read = std::min(static_cast<std::uint64_t>(p_->get_buf.size()),
				p_->uncompressed_total - p_->decompressed_pos);

			auto bytes_read = static_cast<std::uint64_t>(
				p_->backing->sgetn(p_->get_buf.data(), static_cast<std::streamsize>(to_read)));
			if (bytes_read == 0)
				return traits_type::eof();

			p_->decompressed_pos += bytes_read;
			p_->compressed_fed = p_->decompressed_pos;
			setg(p_->get_buf.data(), p_->get_buf.data(), p_->get_buf.data() + bytes_read);
			return traits_type::to_int_type(p_->get_buf[0]);
		}

		// DEFLATE
		auto to_read = std::min(static_cast<std::uint64_t>(p_->compressed_buf.size()),
			p_->compressed_total - p_->compressed_fed);
		if (to_read == 0)
			return traits_type::eof();

		p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
									p_->data_start + p_->compressed_fed)),
			std::ios_base::in);

		auto bytes_read = static_cast<std::uint64_t>(
			p_->backing->sgetn(p_->compressed_buf.data(), static_cast<std::streamsize>(to_read)));
		if (bytes_read == 0)
			return traits_type::eof();

		auto [consumed, produced] =
			p_->inflater->decompress(reinterpret_cast<const std::byte *>(p_->compressed_buf.data()),
				static_cast<std::size_t>(bytes_read),
				reinterpret_cast<std::byte *>(p_->get_buf.data()), p_->get_buf.size());
		if (produced == 0)
			return traits_type::eof();

		p_->compressed_fed += consumed;
		p_->decompressed_pos += produced;
		setg(p_->get_buf.data(), p_->get_buf.data(), p_->get_buf.data() + produced);
		return traits_type::to_int_type(p_->get_buf[0]);
	}

	std::streamsize zip_istreambuf::xsgetn(char_type *s, std::streamsize n) {
		if (n <= 0)
			return 0;

		std::streamsize done = 0;

		// Serve bytes already decompressed into the get area.
		if (auto avail = static_cast<std::streamsize>(egptr() - gptr()); avail > 0) {
			auto take = std::min(avail, n);
			std::memcpy(s, gptr(), static_cast<std::size_t>(take));
			gbump(static_cast<int>(take));
			done += take;
		}

		if (done == n)
			return done;

		// Get area exhausted; mark it empty.
		setg(p_->get_buf.data(), p_->get_buf.data(), p_->get_buf.data());

		if (p_->compression == compression_t::store) {
			auto want = std::min(static_cast<std::uint64_t>(n - done),
				p_->uncompressed_total - p_->decompressed_pos);
			if (want == 0)
				return done;

			p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
										p_->data_start + p_->decompressed_pos)),
				std::ios_base::in);

			auto got = static_cast<std::uint64_t>(
				p_->backing->sgetn(s + done, static_cast<std::streamsize>(want)));

			p_->decompressed_pos += got;
			p_->compressed_fed = p_->decompressed_pos;
			done += static_cast<std::streamsize>(got);
		}
		else {
			// DEFLATE: decompress directly into the caller's buffer.
			while (done < n && p_->decompressed_pos < p_->uncompressed_total) {
				auto to_read = std::min(static_cast<std::uint64_t>(p_->compressed_buf.size()),
					p_->compressed_total - p_->compressed_fed);
				if (to_read == 0)
					break;

				p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
											p_->data_start + p_->compressed_fed)),
					std::ios_base::in);

				auto bytes_read = static_cast<std::uint64_t>(p_->backing->sgetn(
					p_->compressed_buf.data(), static_cast<std::streamsize>(to_read)));
				if (bytes_read == 0)
					break;

				auto [consumed, produced] = p_->inflater->decompress(
					reinterpret_cast<const std::byte *>(p_->compressed_buf.data()),
					static_cast<std::size_t>(bytes_read), reinterpret_cast<std::byte *>(s + done),
					static_cast<std::size_t>(n - done));

				p_->compressed_fed += consumed;
				p_->decompressed_pos += produced;
				done += static_cast<std::streamsize>(produced);

				if (produced == 0)
					break;
			}
		}

		return done;
	}

	void zip_istreambuf::discard(std::uint64_t n) {
		char scratch[4 * 1024];

		while (n > 0) {
			if (p_->compression == compression_t::store) {
				if (p_->decompressed_pos >= p_->uncompressed_total)
					break;

				p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
											p_->data_start + p_->decompressed_pos)),
					std::ios_base::in);

				auto to_read = std::min({ n, static_cast<std::uint64_t>(sizeof(scratch)),
					p_->uncompressed_total - p_->decompressed_pos });

				auto got = static_cast<std::uint64_t>(
					p_->backing->sgetn(scratch, static_cast<std::streamsize>(to_read)));
				if (got == 0)
					break;

				p_->decompressed_pos += got;
				p_->compressed_fed = p_->decompressed_pos;
				n -= got;
			}
			else {
				// DEFLATE
				if (p_->decompressed_pos >= p_->uncompressed_total)
					break;

				auto to_read = std::min(static_cast<std::uint64_t>(p_->compressed_buf.size()),
					p_->compressed_total - p_->compressed_fed);
				if (to_read == 0)
					break;

				p_->backing->pubseekpos(static_cast<std::streampos>(static_cast<std::streamoff>(
											p_->data_start + p_->compressed_fed)),
					std::ios_base::in);

				auto bytes_read = static_cast<std::uint64_t>(p_->backing->sgetn(
					p_->compressed_buf.data(), static_cast<std::streamsize>(to_read)));
				if (bytes_read == 0)
					break;

				auto out_cap = std::min(n, static_cast<std::uint64_t>(sizeof(scratch)));
				auto [consumed, produced] = p_->inflater->decompress(
					reinterpret_cast<const std::byte *>(p_->compressed_buf.data()),
					static_cast<std::size_t>(bytes_read), reinterpret_cast<std::byte *>(scratch),
					static_cast<std::size_t>(out_cap));

				p_->compressed_fed += consumed;
				p_->decompressed_pos += produced;
				n -= produced;

				if (produced == 0 && consumed == 0)
					break;
			}
		}
	}

	zip_istreambuf::pos_type zip_istreambuf::seekoff(off_type off, std::ios_base::seekdir way,
		std::ios_base::openmode which) {
		if (!(which & std::ios_base::in))
			return pos_type(off_type(-1));

		// Current logical read position (end of get area minus unconsumed bytes).
		auto cur = p_->decompressed_pos - static_cast<std::uint64_t>(egptr() - gptr());

		// Fast path: position query (tellg sends seekoff(0, cur)).
		if (off == 0 && way == std::ios_base::cur)
			return static_cast<pos_type>(static_cast<off_type>(cur));

		// Resolve to an absolute target position.
		std::int64_t target_s;
		if (way == std::ios_base::beg) {
			target_s = static_cast<std::int64_t>(off);
		}
		else if (way == std::ios_base::cur) {
			target_s = static_cast<std::int64_t>(cur) + static_cast<std::int64_t>(off);
		}
		else {
			target_s =
				static_cast<std::int64_t>(p_->uncompressed_total) + static_cast<std::int64_t>(off);
		}

		if (target_s < 0)
			target_s = 0;
		auto target = std::min(static_cast<std::uint64_t>(target_s), p_->uncompressed_total);

		// Invalidate the get area.
		setg(p_->get_buf.data(), p_->get_buf.data(), p_->get_buf.data());

		if (p_->compression == compression_t::store) {
			p_->decompressed_pos = target;
			p_->compressed_fed = target;
		}
		else if (target >= p_->decompressed_pos) {
			discard(target - p_->decompressed_pos);
		}
		else {
			p_->inflater->reset();
			p_->compressed_fed = 0;
			p_->decompressed_pos = 0;
			if (target > 0)
				discard(target);
		}

		return static_cast<pos_type>(static_cast<off_type>(p_->decompressed_pos));
	}

	zip_istreambuf::pos_type zip_istreambuf::seekpos(pos_type sp, std::ios_base::openmode which) {
		return seekoff(static_cast<off_type>(sp), std::ios_base::beg, which);
	}

} // namespace zip_xx
