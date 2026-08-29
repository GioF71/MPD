// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LimitedInputStream.hxx"

#include <cassert>
#include <stdexcept>

LimitedInputStream::LimitedInputStream(InputStreamPtr _input,
				       const offset_type max_size)
	:ProxyInputStream(std::move(_input)), remaining(max_size)
{
	ProxyInputStream::Update();
	Check();
}

void
LimitedInputStream::CheckSize()
{
	if (size_checked || !IsReady())
		return;

	if (input->KnownSize() && input->GetRest() > remaining)
		throw std::runtime_error("Input stream is too large");

	size_checked = true;
}

void
LimitedInputStream::Check()
{
	ProxyInputStream::Check();
	CheckSize();
}

std::size_t
LimitedInputStream::Read(std::unique_lock<Mutex> &lock,
			 std::span<std::byte> dest)
{
	assert(!dest.empty());

	if (remaining < dest.size())
		dest = dest.first(static_cast<std::size_t>(remaining) + 1);

	const std::size_t nbytes = input->Read(lock, dest);
	if (nbytes > remaining)
		throw std::runtime_error("Input stream is too large");

	remaining -= nbytes;
	CopyAttributes();
	return nbytes;
}
