// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cassert>
#include <cstddef>
#include <span>

/**
 * A circular buffer.
 *
 * This class does not manage buffer memory.  It will not allocate or
 * free any memory, it only manages the contents of an existing
 * buffer given to the constructor.
 *
 * Everything between #head and #tail is valid data (may wrap around).
 * If both are equal, then the buffer is empty.  Due to this
 * implementation detail, the buffer is empty when #size-1 items are
 * stored; the last buffer cell cannot be used.
 */
template<typename T>
class CircularBuffer {
public:
	using Range = std::span<T>;
	typedef typename Range::pointer pointer;
	typedef typename Range::size_type size_type;

protected:
	/**
	 * The next index to be read.
	 */
	size_type head = 0;

	/**
	 * The next index to be written to.
	 */
	size_type tail = 0;

	const size_type capacity;
	const pointer data;

public:
	constexpr CircularBuffer(pointer _data, size_type _capacity) noexcept
		:capacity(_capacity), data(_data) {}

	CircularBuffer(const CircularBuffer &other) = delete;

protected:
	constexpr size_type Next(size_type i) const noexcept {
		return i + 1 == capacity
			? 0
			: i + 1;
	}

public:
	constexpr void Clear() noexcept {
		head = tail = 0;
	}

	constexpr size_type GetCapacity() const noexcept {
		return capacity;
	}

	constexpr bool empty() const noexcept {
		return head == tail;
	}

	constexpr bool IsFull() const noexcept {
		return Next(tail) == head;
	}

	/**
	 * Returns the number of elements stored in this buffer.
	 */
	constexpr size_type GetSize() const noexcept {
		return head <= tail
			? tail - head
			: capacity - head + tail;
	}

	/**
	 * Returns the number of elements that can be added to this
	 * buffer.
	 */
	constexpr size_type GetSpace() const noexcept {
		/* space = capacity - size - 1 */
		return (head <= tail
			? capacity - tail + head
			: head - tail)
			- 1;
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call Append().
	 */
	constexpr Range Write() noexcept {
		assert(head < capacity);
		assert(tail < capacity);

		size_type end = tail < head
			? head - 1
			/* the "head==0" is there so we don't write
			   the last cell, as this situation cannot be
			   represented by head/tail */
			: capacity - (head == 0);

		return Range(data + tail, end - tail);
	}

	/**
	 * Expands the tail of the buffer, after data has been written
	 * to the buffer returned by Write().
	 */
	constexpr void Append(size_type n) noexcept {
		assert(head < capacity);
		assert(tail < capacity);
		assert(n < capacity);
		assert(tail + n <= capacity);
		assert(head <= tail || tail + n < head);

		tail += n;

		if (tail == capacity) {
			assert(head > 0);
			tail = 0;
		}
	}

	/**
	 * Return a buffer range which may be read.  The buffer pointer is
	 * writable, to allow modifications while parsing.
	 */
	constexpr Range Read() noexcept {
		assert(head < capacity);
		assert(tail < capacity);

		return Range(data + head, (tail < head ? capacity : tail) - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	constexpr void Consume(size_type n) noexcept {
		assert(head < capacity);
		assert(tail < capacity);
		assert(n < capacity);
		assert(head + n <= capacity);
		assert(tail < head || head + n <= tail);

		head += n;
		if (head == capacity)
			head = 0;
	}
};
