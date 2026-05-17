#include <stdint.h>
#include <stdlib.h>

#include <logs.h>
#include <vectors.h>

void __vector_push(struct __Vector* v, const void* data, const size_t size) {
    if (v->len >= v->cap) {
        size_t t_cap = (v->cap + 1) * 2;
        void* aux    = realloc(v->data, size * t_cap);
        if (aux == NULL)
            temporal_suicide_msg("could not resize vector");
        v->data = aux;
        v->cap  = t_cap;
    }
    size_t start = v->len * size;
    for (size_t i = 0; i < size; ++i)
        ((uint8_t*)v->data)[i + start] = ((uint8_t*)data)[i];
    v->len++;
}

void __vector_pop(struct __Vector* v, const size_t size) {
    // probably should clear data but meh
    v->len--;
}

void __vector_reserve(struct __Vector* v, const size_t cap, const size_t size) {
    void* aux = realloc(v->data, size * cap);
    if (aux == NULL)
        temporal_suicide_msg("could not resize vector");
    v->data = aux;
    v->cap  = cap;
}

void __vector_clone(
    struct __Vector* dst, const struct __Vector* const src, const size_t size) {
    dst->data = malloc(size * src->len);
    dst->len  = src->len;
    dst->cap  = src->len;

    for (size_t i = 0; i < size * src->len; ++i)
        ((char*)dst->data)[i] = ((char*)src->data)[i];
}

void __vector_concat(
    struct __Vector* a, const struct __Vector* b, const size_t size) {
    if (b->len == 0)
        return;

    const size_t needed = a->len + b->len;
    if (needed > a->cap) {
        void* aux = realloc(a->data, size * needed);
        if (aux == NULL)
            temporal_suicide_msg("could not resize vector");
        a->data = aux;
        a->cap  = needed;
    }

    const size_t start = a->len * size;
    for (size_t i = 0; i < size * b->len; ++i)
        ((uint8_t*)a->data)[start + i] = ((uint8_t*)b->data)[i];
    a->len = needed;
}
