// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Print.hxx"
#include "Queue.hxx"
#include "Selection.hxx"
#include "song/Filter.hxx"
#include "SongPrint.hxx"
#include "song/DetachedSong.hxx"
#include "song/LightSong.hxx"
#include "tag/Sort.hxx"
#include "client/Response.hxx"
#include "PlaylistError.hxx"

#include <fmt/format.h>

#include <algorithm>

/**
 * Send detailed information about a range of songs in the queue to a
 * client.
 *
 * @param client the client which has requested information
 * @param start the index of the first song (including)
 * @param end the index of the last song (excluding)
 */
static void
queue_print_song_info(Response &r, const Queue &queue,
		      unsigned position)
{
	song_print_info(r, queue.Get(position));
	r.Fmt("Pos: {}\nId: {}\n",
	      position, queue.PositionToId(position));

	uint8_t priority = queue.GetPriorityAtPosition(position);
	if (priority != 0)
		r.Fmt("Prio: {}\n", priority);
}

void
queue_print_info(Response &r, const Queue &queue,
		 unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; ++i)
		queue_print_song_info(r, queue, i);
}

void
queue_print_uris(Response &r, const Queue &queue,
		 unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; ++i) {
		r.Fmt("{}:", i);
		song_print_uri(r, queue.Get(i));
	}
}

void
queue_print_changes_info(Response &r, const Queue &queue,
			 uint32_t version,
			 unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; i++)
		if (queue.IsNewerAtPosition(i, version))
			queue_print_song_info(r, queue, i);
}

void
queue_print_changes_position(Response &r, const Queue &queue,
			     uint32_t version,
			     unsigned start, unsigned end)
{
	assert(start <= end);
	assert(end <= queue.GetLength());

	for (unsigned i = start; i < end; i++)
		if (queue.IsNewerAtPosition(i, version))
			r.Fmt("cpos: {}\nId: {}\n",
			      i, queue.PositionToId(i));
}

[[gnu::pure]]
static std::vector<unsigned>
CollectQueue(const Queue &queue, const QueueSelection &selection) noexcept
{
	std::vector<unsigned> v;

	for (unsigned i = 0; i < queue.GetLength(); i++)
		if (selection.MatchPosition(queue, i))
			v.emplace_back(i);

	return v;
}

static void
PrintSortedQueue(Response &r, const Queue &queue,
		 const QueueSelection &selection)
{
	/* collect all matching songs */
	auto v = CollectQueue(queue, selection);

	auto window = selection.window;
	if (!window.CheckClip(v.size()))
		throw PlaylistError::BadRange();

	/* sort them */
	const auto sort = selection.sort;
	const auto descending = selection.descending;

	if (sort == TagType(SORT_TAG_LAST_MODIFIED))
		std::stable_sort(v.begin(), v.end(),
				 [&queue, descending](unsigned a_pos, unsigned b_pos){
					 if (descending)
						 std::swap(a_pos, b_pos);

					 const auto &a = queue.Get(a_pos);
					 const auto &b = queue.Get(b_pos);

					 return a.GetLastModified() < b.GetLastModified();
				 });
	else if (sort == TagType(SORT_TAG_ADDED))
		std::stable_sort(v.begin(), v.end(),
				 [&queue, descending](unsigned a_pos, unsigned b_pos){
					 if (descending)
						 std::swap(a_pos, b_pos);

					 const auto &a = queue.Get(a_pos);
					 const auto &b = queue.Get(b_pos);

					 return a.GetAdded() < b.GetAdded();
				 });
	else if (sort == TagType(SORT_TAG_PRIO))
		std::stable_sort(v.begin(), v.end(),
				 [&queue, descending](unsigned a_pos, unsigned b_pos){
					 if (descending)
						 std::swap(a_pos, b_pos);

					 return queue.GetPriorityAtPosition(a_pos) <
						 queue.GetPriorityAtPosition(b_pos);
				 });
	else
		std::stable_sort(v.begin(), v.end(),
				 [&queue, sort, descending](unsigned a_pos,
							    unsigned b_pos){
					 const auto &a = queue.Get(a_pos);
					 const auto &b = queue.Get(b_pos);

					 return CompareTags(sort, descending,
							    a.GetTag(),
							    b.GetTag());
				 });

	for (unsigned i = window.start; i < window.end; ++i)
		queue_print_song_info(r, queue, v[i]);
}

void
PrintQueue(Response &r, const Queue &queue,
	   const QueueSelection &selection)
{
	if (selection.sort != TAG_NUM_OF_ITEM_TYPES) {
		PrintSortedQueue(r, queue, selection);
		return;
	}

	auto window = selection.window;

	if (!window.CheckClip(queue.GetLength()))
		throw PlaylistError::BadRange();

	if (selection.window.IsEmpty())
		return;

	unsigned skip = window.start;
	unsigned n = window.Count();

	for (unsigned i = 0; i < queue.GetLength(); i++) {
		if (!selection.MatchPosition(queue, i))
			continue;

		if (skip > 0) {
			--skip;
			continue;
		}

		queue_print_song_info(r, queue, i);

		if (--n == 0)
			break;
	}
}
