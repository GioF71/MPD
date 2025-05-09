// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "InputStream.hxx"
#include "thread/Cond.hxx"
#include "event/InjectEvent.hxx"
#include "util/HugeAllocator.hxx"
#include "util/CircularBuffer.hxx"

#include <cstddef>
#include <exception>

/**
 * Helper class for moving asynchronous (non-blocking) InputStream
 * implementations to the I/O thread.  Data is being read into a ring
 * buffer, and that buffer is then consumed by another thread using
 * the regular #InputStream API.
 */
class AsyncInputStream : public InputStream {
	InjectEvent deferred_resume;
	InjectEvent deferred_seek;

	/**
	 * Signalled when the caller shall be woken up.
	 */
	Cond caller_cond;

	HugeArray<std::byte> allocation;

	CircularBuffer<std::byte> buffer{allocation};
	const size_t resume_at;

	enum class SeekState : uint_least8_t {
		NONE, SCHEDULED, PENDING
	} seek_state = SeekState::NONE;

	bool open = true;

	/**
	 * Is the connection currently paused?  That happens when the
	 * buffer was getting too large.  It will be unpaused when the
	 * buffer is below the threshold again.
	 */
	bool paused = false;

	/**
	 * The #Tag object ready to be requested via
	 * InputStream::ReadTag().
	 */
	std::unique_ptr<Tag> tag;

	offset_type seek_offset;

protected:
	std::exception_ptr postponed_exception;

public:
	AsyncInputStream(EventLoop &event_loop, std::string_view _url,
			 Mutex &_mutex,
			 size_t _buffer_size,
			 size_t _resume_at) noexcept;

	~AsyncInputStream() noexcept override;

	auto &GetEventLoop() const noexcept {
		return deferred_resume.GetEventLoop();
	}

	/* virtual methods from InputStream */
	void Check() final;
	bool IsEOF() const noexcept final;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type new_offset) final;
	std::unique_ptr<Tag> ReadTag() noexcept final;
	bool IsAvailable() const noexcept final;
	size_t Read(std::unique_lock<Mutex> &lock,
		    std::span<std::byte> dest) final;

protected:
	/**
	 * Pass an tag from the I/O thread to the client thread.
	 */
	void SetTag(std::unique_ptr<Tag> _tag) noexcept;
	void ClearTag() noexcept;

	void Pause() noexcept;

	bool IsPaused() const noexcept {
		return paused;
	}

	/**
	 * Declare that the underlying stream was closed.  We will
	 * continue feeding Read() calls from the buffer until it runs
	 * empty.
	 */
	void SetClosed() noexcept {
		open = false;
	}

	bool IsBufferEmpty() const noexcept {
		return buffer.empty();
	}

	bool IsBufferFull() const noexcept {
		return buffer.IsFull();
	}

	/**
	 * Determine how many bytes can be added to the buffer.
	 */
	[[gnu::pure]]
	size_t GetBufferSpace() const noexcept {
		return buffer.GetSpace();
	}

	auto PrepareWriteBuffer() noexcept {
		return buffer.Write();
	}

	void CommitWriteBuffer(size_t nbytes) noexcept;

	/**
	 * Append data to the buffer.  The size must fit into the
	 * buffer; see GetBufferSpace().
	 */
	void AppendToBuffer(std::span<const std::byte> src) noexcept;

	/**
	 * Implement code here that will resume the stream after it
	 * has been paused due to full input buffer.
	 */
	virtual void DoResume() = 0;

	/**
	 * The actual Seek() implementation.  This virtual method will
	 * be called from within the I/O thread.  When the operation
	 * is finished, call SeekDone() to notify the caller.
	 */
	virtual void DoSeek(offset_type new_offset) = 0;

	bool IsSeekPending() const noexcept {
		return seek_state == SeekState::PENDING;
	}

	/**
	 * Call this after seeking has finished.  It will notify the
	 * client thread.
	 */
	void SeekDone() noexcept;

private:
	std::size_t ReadFromBuffer(std::span<std::byte> dest) noexcept;
	void Resume();

	/* for InjectEvent */
	void DeferredResume() noexcept;
	void DeferredSeek() noexcept;
};
