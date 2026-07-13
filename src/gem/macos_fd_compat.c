#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static int set_descriptor_flag(int fd, int flag) {
    int value = fcntl(fd, F_GETFD);
    return value < 0 ? -1 : fcntl(fd, F_SETFD, value | flag);
}

static int set_status_flag(int fd, int flag) {
    int value = fcntl(fd, F_GETFL);
    return value < 0 ? -1 : fcntl(fd, F_SETFL, value | flag);
}

__attribute__((visibility("default"))) int pipe2(int fds[2], int flags) {
    int saved_errno;

    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        errno = EINVAL;
        return -1;
    }
    if (pipe(fds) < 0)
        return -1;
    if (((flags & O_CLOEXEC) && (set_descriptor_flag(fds[0], FD_CLOEXEC) < 0 ||
                                 set_descriptor_flag(fds[1], FD_CLOEXEC) < 0)) ||
        ((flags & O_NONBLOCK) &&
         (set_status_flag(fds[0], O_NONBLOCK) < 0 || set_status_flag(fds[1], O_NONBLOCK) < 0))) {
        saved_errno = errno;
        close(fds[0]);
        close(fds[1]);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

__attribute__((visibility("hidden"))) int dup3(int oldfd, int newfd, int flags) {
    int saved_errno;

    if (flags & ~O_CLOEXEC || oldfd == newfd) {
        errno = EINVAL;
        return -1;
    }
    if (dup2(oldfd, newfd) < 0)
        return -1;
    if ((flags & O_CLOEXEC) && set_descriptor_flag(newfd, FD_CLOEXEC) < 0) {
        saved_errno = errno;
        close(newfd);
        errno = saved_errno;
        return -1;
    }
    return newfd;
}
