#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>

#include <debug.h>
#include <ly_string.h>
#include <path.h>
#include <string.h>
#include <sys/stat.h>
#include <utils.h>

static struct PathSegment build_segment(const struct String name) {
    // not optimal but meaningful!
    if (string_cmp_cstring(name, ".")) {
        return (struct PathSegment){.ty = CURR_PATH, .name = {}};
    }
    if (string_cmp_cstring(name, "..")) {
        return (struct PathSegment){.ty = PREV_PATH, .name = {}};
    }

    return (struct PathSegment){
        .ty   = NAMED_PATH,
        .name = name,
    };
}

bool path_is_dir(const struct Path* const p) {
    char* path_str = get_path_string(*p);
    struct stat s  = {};
    stat(path_str, &s);
    bool isdir = S_ISDIR(s.st_mode);

    return isdir;
}

void push_name(struct Path* path, const char* name) {
    size_t len                 = strlen(name);
    struct String nname        = string_read_n(name, len);
    struct PathSegment segment = build_segment(nname);

    vector_push(path->_inner, segment);
}

void push_name_cstring(struct Path* path, const char* name) {
    struct PathSegment segment = build_segment(string_from_cstr(name));

    vector_push(path->_inner, segment);
}

void push_name_string(struct Path* path, struct String name) {
    struct PathSegment segment = build_segment(name);

    vector_push(path->_inner, segment);
}

void push_segment(struct Path* path, const struct PathSegment segment) {
    vector_push(path->_inner, segment);
}

struct Path root_path() {
    struct PathSegment segment = {.ty = ROOT_PATH, .name = {}};

    struct Path path = {};
    push_segment(&path, segment);

    return path;
}

static struct PathSegment make_segment(
    struct Path path, struct String cs, size_t beg, size_t end) {
    struct String name = substring(cs, beg, end);

    return build_segment(name);
}

struct Path parse_path(const char* path) {
    struct Path _path = {};

    size_t start = 0;
    size_t i     = 0;

    if (path[0] == '/') {
        _path = root_path();
        start = 1;
        i     = 1;
    }

    struct String cs = string_read_n(path, strlen(path));

    for (; i < cs.len; ++i) {
        if (cs.data[i] == '/') {
            push_segment(&_path, make_segment(_path, cs, start, i));

            start = i + 1;
        }
    }

    if (start < cs.len) {
        push_segment(&_path, make_segment(_path, cs, start, cs.len));
    }

    free(cs.data);
    return _path;
}

struct Path clone_path(struct Path* src) {
    struct Path cpy = {};
    size_t len      = src->_inner.len;
    vector_reserve(cpy._inner, len);
    for (size_t i = 0; i < len; ++i) {
        if (src->_inner.data[i].ty != NAMED_PATH) {
            push_segment(&cpy,
                (struct PathSegment){.ty = src->_inner.data[i].ty, .name = {}});
            continue;
        }

        struct String name = {};
        vector_clone(name, src->_inner.data[i].name);
        push_name_string(&cpy, name);
    }
    return cpy;
}

struct String get_name(struct Path* path) {
    struct PathSegment last = path->_inner.data[path->_inner.len - 1];
    if (last.ty != NAMED_PATH)
        return (struct String){};

    struct String name = {};
    vector_clone(name, last.name);
    return name;
}

TEST(get_path_name) {
    struct Path path   = parse_path("some/path/foo");
    struct String name = get_name(&path);
    ASSERT(string_cmp_cstring(name, "foo"), "path name is not foo");
    return 0;
}

void expand_path(struct Path* self, const struct Path* const cwd) {
    if (self->_inner.data[0].ty == CURR_PATH) {
        struct Path cpy = {};

        for (size_t i = 0; i < cwd->_inner.len; ++i) {
            struct PathSegment segment = {};
            segment.ty                 = cwd->_inner.data[i].ty;
            vector_clone(segment.name, cwd->_inner.data[i].name);

            push_segment(&cpy, segment);
        }

        for (size_t i = 1; i < self->_inner.len; ++i) {
            struct PathSegment segment = {};
            segment.ty                 = self->_inner.data[i].ty;
            vector_clone(segment.name, self->_inner.data[i].name);

            push_segment(&cpy, segment);
        }

        destruct_path(self);

        *self = cpy;
    }
    return;
}

char* get_path_string(const struct Path path) {
    struct String str = {};
    vector_reserve(str, path._inner.len * 5);
    for (size_t i = 0; i < path._inner.len; ++i) {
        switch (path._inner.data[i].ty) {
        case ROOT_PATH:
            break;
        case CURR_PATH:
            vector_push(str, '.');
            break;
        case PREV_PATH:
            vector_push(str, '.');
            vector_push(str, '.');
            break;
        case NAMED_PATH: {
            char* start = string_to_cstring(path._inner.data[i].name);
            for (char* inn = start; *inn != 0; ++inn) vector_push(str, *inn);

            free(start);
        } break;
        }
        vector_push(str, '/');
    }
    vector_pop(str);
    vector_push(str, '\0');
    size_t len = str.len;
    char* d    = realloc(str.data, len);

    return d;
}

void pop_segment(struct Path* path) {
    if (path->_inner.len < 1) {
        return;
    }

    struct PathSegment end = path->_inner.data[path->_inner.len - 1];
    free(end.name.data);

    vector_pop(path->_inner);
}

void destruct_path(struct Path* path) {
    for (size_t i = 0; i < path->_inner.len; ++i) {
        struct PathSegment seg = path->_inner.data[i];
        free(seg.name.data);
    }
    free(path->_inner.data);
    path->_inner.data = NULL;
    path->_inner.len  = 0;
}

struct VectorPath get_childs(struct Path* path) {
    struct VectorPath p = {};
    char* name          = get_path_string(*path);
    DIR* d              = opendir(name);
    if (d == NULL) {
        return p;
    }

    struct dirent* entry = NULL;

    char* tmp = get_path_string(*path);
    free(tmp);
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (strcmp(entry->d_name, ".") == 0) {
            continue;
        }
        struct Path clone = clone_path(path);
        push_name(&clone, entry->d_name);

        vector_push(p, clone)
    }

    return p;
}

TEST(path_parsing) {
    struct Path path = parse_path("/test/path");
    ASSERT(path._inner.data[0].ty == ROOT_PATH, "root path not detected");
    ASSERT(path._inner.data[1].ty == NAMED_PATH,
        "'test' part not detected as named");
    ASSERT(path._inner.data[2].ty == NAMED_PATH,
        "'path' part not detected as named");
    return 0;
}
TEST(path_clonning) {
    struct Path path = parse_path("/test/path");
    struct Path copy = clone_path(&path);
    for (size_t i = 0; i < path._inner.len; ++i) {
        ASSERT(path._inner.data != copy._inner.data,
            "copy just shared the pointer instead of copying");

        ASSERT(string_cmp(path._inner.data[i].name, copy._inner.data[i].name),
            "path segments are different...");
    }
    return 0;
}
