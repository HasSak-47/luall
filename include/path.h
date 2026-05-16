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

#include "vectors.h"

DefineVector(InnerVectorPath, struct PathSegment);

struct Path {
    // NOTE: PRIVATE/PROTECTED!!
    struct InnerVectorPath _inner;
};

char* path_get_string(const struct Path path);
void path_push_name(struct Path* path, const char* name);
bool path_is_dir(const struct Path* const p);
bool path_mkdir_p(const struct Path* const p);
/* takes ownership of the string */
void path_push_name_string(struct Path* path, struct String name);
void path_push_segment(struct Path* path, const struct PathSegment segment);
void path_pop_segment(struct Path* path);

void path_expand(struct Path* self, const struct Path* const cwd);

struct Path path_root();
struct Path path_parse(const char* path);

void path_destruct(struct Path* path);
struct Path path_clone(struct Path* path);
struct VectorPath path_get_childs(struct Path* path);
/*
 * returns a copy of the last path segment if it is a named type, empty string
 * if it is not
 */
struct String path_get_name(struct Path* path);

DefineVector(VectorPath, struct Path);

#endif // !__PATH__
