// Diagnostic LD_PRELOAD shim, NOT a complete memfd implementation.
// Regular files cannot emulate seals: requests to apply seals will still fail.
// Use only to isolate Mesa allocation failures in the asset-free Vulkan probe.
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

int memfd_create(const char *name, unsigned int flags)
{
    int fd = (int)syscall(SYS_memfd_create, name, flags);
    if (fd >= 0 || errno != ENOSYS) return fd;
    if (flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING))
    {
        errno = EINVAL;
        return -1;
    }
    char path[] = "/tmp/mw3-mesa-probe-XXXXXX";
    fd = mkstemp(path);
    if (fd < 0) return -1;
    if (unlink(path) != 0 ||
        ((flags & MFD_CLOEXEC) && fcntl(fd, F_SETFD, FD_CLOEXEC) != 0))
    {
        int error = errno;
        close(fd);
        errno = error;
        return -1;
    }
    return fd;
}
