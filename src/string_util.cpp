/*

Copyright (c) 2012-2016, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include "libtorrent/config.hpp"
#include "libtorrent/string_util.hpp"
#include "libtorrent/random.hpp"
#include "libtorrent/error_code.hpp"
#include "libtorrent/parse_url.hpp"
#include "libtorrent/address.hpp"
#include "libtorrent/assert.hpp"

#include <cstdlib> // for malloc
#include <cstring> // for memmov/strcpy/strlen
#include <algorithm> // for search

namespace libtorrent {

	// lexical_cast's result depends on the locale. We need
	// a well defined result
	std::array<char, 4 + std::numeric_limits<std::int64_t>::digits10>
		to_string(std::int64_t const n)
	{
		std::array<char, 4 + std::numeric_limits<std::int64_t>::digits10> ret;
		char *p = &ret.back();
		*p = '\0';
		std::uint64_t un = std::uint64_t(n);
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4146 ) /* warning C4146: unary minus operator applied to unsigned type */
#endif // _MSC_VER
		if (n < 0)  un = -un;
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER
		do {
			*--p = '0' + un % 10;
			un /= 10;
		} while (un);
		if (n < 0) *--p = '-';
		std::memmove(ret.data(), p, std::size_t(&ret.back() - p + 1));
		return ret;
	}

	bool is_alpha(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool is_print(char c)
	{
		return c >= 32 && c < 127;
	}

	bool is_space(char c)
	{
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
	}

	char to_lower(char c)
	{
		return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
	}

	int split_string(char const** tags, int buf_size, char* in)
	{
		int ret = 0;
		char* i = in;
		for (;*i; ++i)
		{
			if (!is_print(*i) || is_space(*i))
			{
				*i = 0;
				if (ret == buf_size) return ret;
				continue;
			}
			if (i == in || i[-1] == 0)
			{
				tags[ret++] = i;
			}
		}
		return ret;
	}

	bool string_begins_no_case(char const* s1, char const* s2)
	{
		TORRENT_ASSERT(s1 != nullptr);
		TORRENT_ASSERT(s2 != nullptr);

		while (*s1 != 0)
		{
			if (to_lower(*s1) != to_lower(*s2)) return false;
			++s1;
			++s2;
		}
		return true;
	}

	bool string_equal_no_case(string_view s1, string_view s2)
	{
		if (s1.size() != s2.size()) return false;
		return std::equal(s1.begin(), s1.end(), s2.begin()
			, [] (char const c1, char const c2)
			{ return to_lower(c1) == to_lower(c2); });
	}

	// generate a url-safe random string
	void url_random(char* begin, char* end)
	{
		// http-accepted characters:
		// excluding ', since some buggy trackers don't support that
		static char const printable[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz-_.!~*()";

		// the random number
		while (begin != end)
			*begin++ = printable[random(sizeof(printable) - 2)];
	}

	bool string_ends_with(string_view s1, string_view s2)
	{
		return s1.size() >= s2.size() && std::equal(s2.rbegin(), s2.rend(), s1.rbegin());
	}

	int search(span<char const> src, span<char const> target)
	{
		TORRENT_ASSERT(!src.empty());
		TORRENT_ASSERT(!target.empty());
		TORRENT_ASSERT(target.size() >= src.size());
		TORRENT_ASSERT(target.size() < std::size_t(std::numeric_limits<int>::max()));

		auto it = std::search(target.begin(), target.end(), src.begin(), src.end());

		// no complete sync
		if (it == target.end()) return -1;
		return static_cast<int>(it - target.begin());
	}

	char* allocate_string_copy(char const* str)
	{
		if (str == nullptr) return nullptr;
		std::size_t const len = std::strlen(str);
		char* tmp = static_cast<char*>(std::malloc(len + 1));
		if (tmp == nullptr) return nullptr;
		std::memcpy(tmp, str, len);
		tmp[len] = '\0';
		return tmp;
	}

	std::string print_listen_interfaces(std::vector<listen_interface_t> const& in)
	{
		std::string ret;
		for (auto const& i : in)
		{
			if (!ret.empty()) ret += ',';

#if TORRENT_USE_IPV6
			error_code ec;
			address_v6::from_string(i.device, ec);
			if (!ec)
			{
				// IPv6 addresses must be wrapped in square brackets
				ret += '[';
				ret += i.device;
				ret += ']';
			}
			else
#endif
			{
				ret += i.device;
			}
			ret += ':';
			ret += to_string(i.port).data();
			if (i.ssl) ret += 's';
		}

		return ret;
	}

	// this parses the string that's used as the listen_interfaces setting.
	// it is a comma-separated list of IP or device names with ports. For
	// example: "eth0:6881,eth1:6881" or "127.0.0.1:6881"
	std::vector<listen_interface_t> parse_listen_interfaces(std::string const& in)
	{
		std::vector<listen_interface_t> out;

		std::string::size_type start = 0;

		while (start < in.size())
		{
			// skip leading spaces
			while (start < in.size() && is_space(in[start]))
				++start;

			if (start == in.size()) return out;

			listen_interface_t iface;
			iface.ssl = false;

#if !TORRENT_USE_IPV6
			bool ipv6 = false;
#endif
			if (in[start] == '[')
			{
#if !TORRENT_USE_IPV6
				ipv6 = true;
#endif
				++start;
				// IPv6 address
				while (start < in.size() && in[start] != ']')
					iface.device += in[start++];

				// skip to the colon
				while (start < in.size() && in[start] != ':')
					++start;
			}
			else
			{
				// consume device name
				while (start < in.size() && !is_space(in[start]) && in[start] != ':')
					iface.device += in[start++];
			}

			// skip spaces
			while (start < in.size() && is_space(in[start]))
				++start;

			if (start == in.size() || in[start] != ':') return out;
			++start; // skip colon

			// skip spaces
			while (start < in.size() && is_space(in[start]))
				++start;

			// consume port
			std::string port;
			while (start < in.size() && is_digit(in[start]) && in[start] != ',')
				port += in[start++];

			if (port.empty() || port.size() > 5)
			{
				iface.port = -1;
			}
			else
			{
				iface.port = std::atoi(port.c_str());
				if (iface.port < 0 || iface.port > 65535) iface.port = -1;
			}

			// skip spaces
			while (start < in.size() && is_space(in[start]))
				++start;

			// consume potential SSL 's'
			if (start < in.size() && in[start] == 's')
			{
				iface.ssl = true;
				++start;
			}

			// skip until end or comma
			while (start < in.size() && in[start] != ',')
				++start;

			if (iface.port >= 0
#if !TORRENT_USE_IPV6
				&& ipv6 == false
#endif
				)
			{
				out.push_back(iface);
			}

			// skip the comma
			if (start < in.size() && in[start] == ',')
				++start;

		}

		return out;
	}

	// this parses the string that's used as the listen_interfaces setting.
	// it is a comma-separated list of IP or device names with ports. For
	// example: "eth0:6881,eth1:6881" or "127.0.0.1:6881"
	void parse_comma_separated_string_port(std::string const& in
		, std::vector<std::pair<std::string, int>>& out)
	{
		out.clear();

		std::string::size_type start = 0;
		std::string::size_type end = 0;

		while (start < in.size())
		{
			// skip leading spaces
			while (start < in.size()
				&& is_space(in[start]))
				++start;

			end = in.find_first_of(',', start);
			if (end == std::string::npos) end = in.size();

			std::string::size_type colon = in.find_last_of(':', end);

			if (colon != std::string::npos && colon > start)
			{
				int port = atoi(in.substr(colon + 1, end - colon - 1).c_str());

				// skip trailing spaces
				std::string::size_type soft_end = colon;
				while (soft_end > start
					&& is_space(in[soft_end-1]))
					--soft_end;

				// in case this is an IPv6 address, strip off the square brackets
				// to make it more easily parseable into an ip::address
				if (in[start] == '[') ++start;
				if (soft_end > start && in[soft_end-1] == ']') --soft_end;

				out.push_back(std::make_pair(in.substr(start, soft_end - start), port));
			}

			start = end + 1;
		}
	}

	void parse_comma_separated_string(std::string const& in, std::vector<std::string>& out)
	{
		out.clear();

		std::string::size_type start = 0;
		std::string::size_type end = 0;

		while (start < in.size())
		{
			// skip leading spaces
			while (start < in.size()
				&& is_space(in[start]))
				++start;

			end = in.find_first_of(',', start);
			if (end == std::string::npos) end = in.size();

			// skip trailing spaces
			std::string::size_type soft_end = end;
			while (soft_end > start
				&& is_space(in[soft_end-1]))
				--soft_end;

			out.push_back(in.substr(start, soft_end - start));
			start = end + 1;
		}
	}

	std::pair<string_view, string_view> split_string(string_view last, char const sep)
	{
		if (last.empty()) return {{}, {}};

		std::size_t pos = 0;
		if (last[0] == '"' && sep != '"')
		{
			for (auto const c : last.substr(1))
			{
				++pos;
				if (c == '"') break;
			}
		}
		std::size_t found_sep = 0;
		for (char const c : last.substr(pos))
		{
			if (c == sep)
			{
				found_sep = 1;
				break;
			}
			++pos;
		}
		return {last.substr(0, pos), last.substr(pos + found_sep)};
	}

#if TORRENT_USE_I2P

	bool is_i2p_url(std::string const& url)
	{
		using std::ignore;
		std::string hostname;
		error_code ec;
		std::tie(ignore, ignore, hostname, ignore, ignore)
			= parse_url_components(url, ec);
		return string_ends_with(hostname, ".i2p");
	}

#endif

	std::size_t string_hash_no_case::operator()(std::string const& s) const
	{
		std::size_t ret = 5381;
		for (std::string::const_iterator i = s.begin(); i != s.end(); ++i)
			ret = (ret * 33) ^ static_cast<std::size_t>(to_lower(*i));
		return ret;
	}

	bool string_eq_no_case::operator()(std::string const& lhs, std::string const& rhs) const
	{
		if (lhs.size() != rhs.size()) return false;

		std::string::const_iterator s1 = lhs.begin();
		std::string::const_iterator s2 = rhs.begin();

		while (s1 != lhs.end() && s2 != rhs.end())
		{
			if (to_lower(*s1) != to_lower(*s2)) return false;
			++s1;
			++s2;
		}
		return true;
	}
}
