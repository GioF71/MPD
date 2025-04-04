// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "Open.hxx"
#include "UniqueFileDescriptor.hxx"
#include "lib/fmt/SystemError.hxx"

#ifdef __linux__
#include "system/linux/Features.h"
#ifdef HAVE_OPENAT2
#include "FileAt.hxx"
#include "system/linux/openat2.h"
#endif // HAVE_OPENAT2
#endif // __linux__

#include <fcntl.h>

UniqueFileDescriptor
OpenReadOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_RDONLY|flags))
		throw FmtErrno("Failed to open {:?}", path);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_WRONLY|flags))
		throw FmtErrno("Failed to open {:?}", path);

	return fd;
}

#ifndef _WIN32

UniqueFileDescriptor
OpenDirectory(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_DIRECTORY|O_RDONLY|flags))
		throw FmtErrno("Failed to open {:?}", path);

	return fd;
}

#endif

#ifdef __linux__

UniqueFileDescriptor
OpenPath(const char *path, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(path, O_PATH|flags))
		throw FmtErrno("Failed to open {:?}", path);

	return fd;
}

UniqueFileDescriptor
OpenPath(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_PATH|flags))
		throw FmtErrno("Failed to open {:?}", name);

	return fd;
}

UniqueFileDescriptor
OpenReadOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_RDONLY|flags))
		throw FmtErrno("Failed to open {:?}", name);

	return fd;
}

UniqueFileDescriptor
OpenWriteOnly(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_WRONLY|flags))
		throw FmtErrno("Failed to open {:?}", name);

	return fd;
}

UniqueFileDescriptor
OpenDirectory(FileDescriptor directory, const char *name, int flags)
{
	UniqueFileDescriptor fd;
	if (!fd.Open(directory, name, O_DIRECTORY|O_RDONLY|flags))
		throw FmtErrno("Failed to open {:?}", name);

	return fd;
}

#ifdef HAVE_OPENAT2

UniqueFileDescriptor
TryOpen(FileAt file, const struct open_how &how) noexcept
{
	int fd = openat2(file.directory.Get(), file.name, &how, sizeof(how));
	return UniqueFileDescriptor{AdoptTag{}, fd};
}

UniqueFileDescriptor
Open(FileAt file, const struct open_how &how)
{
	auto fd = TryOpen(file, how);
	if (!fd.IsDefined())
		throw FmtErrno("Failed to open {:?}", file.name);

	return fd;
}

#endif // HAVE_OPENAT2

#endif // __linux__
