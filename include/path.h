#ifndef __PATH__
#define __PATH__

#include "ly_string.h"

enum SegmentType {
    ROOT_PATH,
    PREV_PATH,
    CURR_PATH,
    NAMED_PATH,
};

struct PathSegment {
    struct String name;
    enum SegmentType ty;
};

#include "utils.h"

DefineVector(InnerVectorPath, struct PathSegment);

struct Path {
    // NOTE: PRIVATE/PROTECTED!!
    struct InnerVectorPath _inner;
};

char* get_path_string(const struct Path path);
void push_name(struct Path* path, const char* name);
bool path_is_dir(const struct Path* const p);
/* takes ownership of the string */
void push_name_string(struct Path* path, struct String name);
void push_segment(struct Path* path, const struct PathSegment segment);
void pop_segment(struct Path* path);

void expand_path(struct Path* self, const struct Path* const cwd);

struct Path root_path();
struct Path parse_path(const char* path);

void destruct_path(struct Path* path);
struct Path clone_path(struct Path* path);
struct VectorPath get_childs(struct Path* path);
/*
 * returns a copy of the last path segment if it is a named type, empty string
 * if it is not
 */
struct String get_name(struct Path* path);

DefineVector(VectorPath, struct Path);

#endif // !__PATH__
