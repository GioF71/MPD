// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "input/Offset.hxx"

/* XML playlist plugins materialize the document before returning songs. */
static constexpr offset_type XML_PLAYLIST_MAX_SIZE = 16 * 1024 * 1024;
