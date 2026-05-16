#ifndef __VECTORS_H__
#define __VECTORS_H__

#include <stddef.h>

struct __Vector {
    void* data;
    size_t len;
    size_t cap;
};

#define DefineVector(Name, T)                                                  \
    struct Name {                                                              \
        T* data;                                                               \
        size_t len;                                                            \
        size_t cap;                                                            \
    };

void __vector_push(struct __Vector* v, const void* data, const size_t size);
void __vector_pop(struct __Vector* v, const size_t size);
void __vector_reserve(struct __Vector* v, const size_t cap, const size_t size);
void __vector_clone(
    struct __Vector* dst, const struct __Vector* const src, const size_t size);
void __vector_concat(
    struct __Vector* a, const struct __Vector* b, const size_t size);

#define vector_push(__vector, __v_data)                                        \
    {                                                                          \
        const typeof(*(__vector).data) __vector_data = (__v_data);             \
        __vector_push((struct __Vector*)&(__vector), &__vector_data,           \
            sizeof(*(__vector).data));                                         \
    }

#define vector_clone(__dst, __src)                                             \
    {                                                                          \
        __vector_clone((struct __Vector*)&(__dst), (struct __Vector*)&(__src), \
            sizeof(*(__src).data));                                            \
    }

#define vector_pop(vector)                                                     \
    __vector_pop((struct __Vector*)&(vector), sizeof(*(vector).data))

#define vector_reserve(vector, cap)                                            \
    __vector_reserve((struct __Vector*)&(vector), cap, sizeof(*(vector).data))

#define vector_concat(__a, __b)                                                \
    __vector_concat(                                                           \
        (struct __Vector*)&__a, (struct __Vector*)&(__b), sizeof(*(__a).data))

#endif
