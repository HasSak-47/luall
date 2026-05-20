#include <errno.h>
#include <lauxlib.h>
#include <lua.h>

#include <io_api.h>
#include <path.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "logs.h"

struct FileHandler stdout_handler() {
    return (struct FileHandler){
        STDOUT_FILENO,
        OPEN_MODE_WRITE,
        false,
    };
}

struct FileHandler stderr_handler() {
    return (struct FileHandler){
        STDERR_FILENO,
        OPEN_MODE_WRITE,
        false,
    };
}

struct FileHandler file_handler_open(struct Path path, enum OpenMode flags) {
    char* file = path_get_string(path);
    int o_flag = (flags & OPEN_MODE_CREATE ? O_CREAT : 0);
    // yo unix wtf
    if ((flags & OPEN_MODE_READ) && (flags & OPEN_MODE_WRITE)) {
        o_flag += O_RDWR;
    }
    else if ((flags & OPEN_MODE_READ)) {
        o_flag += O_RDONLY;
    }
    else if ((flags & OPEN_MODE_WRITE)) {
        o_flag += O_WRONLY;
    }
    if (flags & OPEN_MODE_APPEND) {
        o_flag += O_APPEND;
    }
    if (flags & OPEN_MODE_TRUNC) {
        o_flag += O_TRUNC;
    }
    int mask = umask(0);
    umask(mask);

    int fd = open(file, o_flag, 0666 & ~mask);
    free(file);
    return (struct FileHandler){
        fd,
        flags,
        true,
    };
}

void file_handler_close(struct FileHandler handler) {
    if (handler.should_close) {
        close(handler.fd);
    }
}

struct Pipe pipe_new() {
    struct Pipe p = {.p = {-1, -1}};
    int r         = pipe(p.p);
    if (r < 0)
        temporal_suicide_msg("failed to create new pipe");

    return p;
}

void pipe_close(struct Pipe* p) {
    if (p->p[0] != -1) {
        close(p->p[0]);
        p->p[0] = -1;
    }
    if (p->p[1] != -1) {
        close(p->p[1]);
        p->p[1] = -1;
    }
}

#define BUFFER_LEN 256

struct String pipe_read(struct Pipe* p) {
    struct String str       = {};
    char buffer[BUFFER_LEN] = {};
    size_t bytes_read       = read(p->p[0], buffer, BUFFER_LEN);

    size_t iters = 0;
    while (bytes_read != 0) {
        vector_reserve(str, str.cap + BUFFER_LEN);
        for (size_t i = 0; i < BUFFER_LEN; ++i) {
            str.data[iters * BUFFER_LEN + i] = buffer[i];
        }
        iters += 1;
        bytes_read = read(p->p[0], buffer, BUFFER_LEN);
    }

    return str;
}

void pipe_write(struct Pipe* p, struct String data) {
    write(p->p[1], data.data, data.len);
}
