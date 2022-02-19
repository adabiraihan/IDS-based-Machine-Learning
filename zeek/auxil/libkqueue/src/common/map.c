/*
 * Copyright (c) 2011 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "private.h"

struct map {
    size_t len;
    void **data;
};

struct map *
map_new(size_t len)
{
    struct map *dst;

    dst = calloc(1, sizeof(struct map));
    if (dst == NULL)
        return (NULL);
#ifdef _WIN32
    dst->data = calloc(len, sizeof(void*));
    if(dst->data == NULL) {
        dbg_perror("calloc()");
        free(dst);
        return NULL;
    }
    dst->len = len;
#else
    dst->data = mmap(NULL, len * sizeof(void *), PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_NORESERVE | MAP_ANON, -1, 0);
    if (dst->data == MAP_FAILED) {
        dbg_perror("mmap(2)");
        free(dst);
        return (NULL);
    }
    dst->len = len;
#endif

    return (dst);
}

int
map_insert(struct map *m, int idx, void *ptr)
{
    if (unlikely(idx < 0 || idx > (int)m->len))
           return (-1);

    if (atomic_ptr_cas(&(m->data[idx]), 0, ptr) == NULL) {
        dbg_printf("idx=%i - inserted ptr=%p into map", idx, ptr);
        return (0);
    } else {
        dbg_printf("idx=%i - tried to insert ptr=%p into a non-empty location (cur_ptr=%p)",
                   idx, ptr, m->data[idx]);
        return (-1);
    }
}

int
map_remove(struct map *m, int idx, void *ptr)
{
    if (unlikely(idx < 0 || idx > (int)m->len))
           return (-1);

    if (atomic_ptr_cas(&(m->data[idx]), ptr, 0) == NULL) {
        dbg_printf("idx=%i - removed ptr=%p from map", idx, ptr);
        return (0);
    } else {
        dbg_printf("idx=%i - removal failed, ptr=%p != cur_ptr=%p", idx, ptr, m->data[idx]);
        return (-1);
    }
}

int
map_replace(struct map *m, int idx, void *oldp, void *newp)
{
    void *tmp;

    if (unlikely(idx < 0 || idx > (int)m->len))
           return (-1);

    tmp = atomic_ptr_cas(&(m->data[idx]), oldp, newp);
    if (tmp == oldp) {
        dbg_printf("idx=%i - replaced item in map with ptr=%p", idx, newp);

        return (0);
    } else {
        dbg_printf("idx=%i - replace failed, ptr=%p != cur_ptr=%p", idx, newp, m->data[idx]);
        return (-1);
    }
}

void *
map_lookup(struct map *m, int idx)
{
    if (unlikely(idx < 0 || idx > (int)m->len))
        return (NULL);

    return m->data[idx];
}

void *
map_delete(struct map *m, int idx)
{
    void *oval;
    void *nval;

    if (unlikely(idx < 0 || idx > (int)m->len))
           return ((void *)-1);

    /* Hopefully we aren't racing with another thread, but you never know.. */
    do {
        oval = m->data[idx];
        nval = atomic_ptr_cas(&(m->data[idx]), oval, NULL);
    } while (nval != oval);

    m->data[idx] = NULL;

    return ((void *) oval);
}
