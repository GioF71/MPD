// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "ProxyInputStream.hxx"

/**
 * An #InputStream proxy which limits the amount of data that may be
 * read from the underlying stream.
 */
class LimitedInputStream final : public ProxyInputStream {
	offset_type remaining;
	bool size_checked = false;

	void CheckSize();

public:
	LimitedInputStream(InputStreamPtr _input, offset_type max_size);

	/* virtual methods from class InputStream */
	void Check() override;
	std::size_t Read(std::unique_lock<Mutex> &lock,
			 std::span<std::byte> dest) override;
};
