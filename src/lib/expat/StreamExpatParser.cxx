// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ExpatParser.hxx"
#include "input/InputStream.hxx"
#include "util/SpanCast.hxx"

void
ExpatParser::Parse(InputStream &is, const offset_type max_size)
{
	assert(is.IsReady());

	if (is.KnownSize() && is.GetRest() > max_size)
		throw std::runtime_error("XML document is too large");

	offset_type total_size = 0;
	while (true) {
		std::byte buffer[4096];
		size_t nbytes = is.LockRead(buffer);
		if (nbytes == 0)
			break;

		if (nbytes > max_size - total_size)
			throw std::runtime_error("XML document is too large");

		total_size += nbytes;
		Parse(ToStringView(std::span{buffer}.first(nbytes)));
	}

	CompleteParse();
}
