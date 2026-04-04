// cbuf.h - generic circular buffer (byte-copy based)
// Usage example at bottom.

#ifndef CBUF_H
#define CBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t *buf;          // backing storage
    size_t   cap;          // number of elements
    size_t   elem_size;    // bytes per element
    size_t   head;         // next write index
    size_t   tail;         // next read index
    size_t   count;        // elements currently stored
} cbuf_t;

// Initialize with caller-provided storage:
//   storage_bytes must be at least cap * elem_size
static inline void cbuf_init(cbuf_t *cb,
                             void *storage_bytes,
                             size_t cap,
                             size_t elem_size)
{
    cb->buf = (uint8_t *)storage_bytes;
    cb->cap = cap;
    cb->elem_size = elem_size;
    cb->head = 0;
    cb->tail = 0;
    cb->count = 0;
}

static inline void cbuf_clear(cbuf_t *cb)
{
    cb->head = cb->tail = cb->count = 0;
}

static inline size_t cbuf_capacity(const cbuf_t *cb) { return cb->cap; }
static inline size_t cbuf_size(const cbuf_t *cb)     { return cb->count; }
static inline bool   cbuf_empty(const cbuf_t *cb)    { return cb->count == 0; }
static inline bool   cbuf_full(const cbuf_t *cb)     { return cb->count == cb->cap; }

static inline void* cbuf_at_(const cbuf_t *cb, size_t index)
{
    // index is element index (0..cap-1)
    return (void *)(cb->buf + (index * cb->elem_size));
}

// Push without overwrite: returns false if full
static inline bool cbuf_push(cbuf_t *cb, const void *elem)
{
    if (cbuf_full(cb)) return false;

    memcpy(cbuf_at_(cb, cb->head), elem, cb->elem_size);
    cb->head = (cb->head + 1) % cb->cap;
    cb->count++;
    return true;
}

// Push with overwrite: if full, overwrites oldest element and advances tail.
// Returns true always (unless cap==0 / elem_size==0, which you should avoid).
static inline bool cbuf_push_overwrite(cbuf_t *cb, const void *elem)
{
    if (cb->cap == 0 || cb->elem_size == 0) return false;

    if (cbuf_full(cb)) {
        // drop the oldest
        cb->tail = (cb->tail + 1) % cb->cap;
        cb->count--; // will re-increment below
    }

    memcpy(cbuf_at_(cb, cb->head), elem, cb->elem_size);
    cb->head = (cb->head + 1) % cb->cap;
    cb->count++;
    return true;
}

// Pop: copies oldest element into out_elem. Returns false if empty.
static inline bool cbuf_pop(cbuf_t *cb, void *out_elem)
{
    if (cbuf_empty(cb)) return false;

    if (out_elem) {
        memcpy(out_elem, cbuf_at_(cb, cb->tail), cb->elem_size);
    }
    cb->tail = (cb->tail + 1) % cb->cap;
    cb->count--;
    return true;
}

// Pop (back): copies most recently pushed element into out_elem. Returns false if empty.
static inline bool cbuf_pop_back(cbuf_t *cb, void *out_elem)
{
    if (cbuf_empty(cb)) return false;

    // Move head back to the last valid element
    cb->head = (cb->head == 0) ? (cb->cap - 1) : (cb->head - 1);

    if (out_elem) {
        memcpy(out_elem, cbuf_at_(cb, cb->head), cb->elem_size);
    }

    cb->count--;
    return true;
}

// Peek (front): copies oldest element into out_elem without removing. False if empty.
static inline bool cbuf_peek(const cbuf_t *cb, void *out_elem)
{
    if (cbuf_empty(cb)) return false;
    if (out_elem) {
        memcpy(out_elem, cbuf_at_(cb, cb->tail), cb->elem_size);
    }
    return true;
}

// Peek (back): copies most recently pushed element into out_elem. False if empty.
static inline bool cbuf_peek_back(const cbuf_t *cb, void *out_elem)
{
    if (cbuf_empty(cb)) return false;

    size_t last = (cb->head == 0) ? (cb->cap - 1) : (cb->head - 1);
    if (out_elem) {
        memcpy(out_elem, cbuf_at_(cb, last), cb->elem_size);
    }
    return true;
}

#endif // CBUF_H

/* ---------------- Example ----------------
#include "cbuf.h"
#include <stdio.h>

int main(void)
{
    int storage[8];
    cbuf_t q;
    cbuf_init(&q, storage, 8, sizeof(int));

    for (int i=0;i<10;i++) cbuf_push_overwrite(&q, &i);

    while (!cbuf_empty(&q)) {
        int x;
        cbuf_pop(&q, &x);
        printf("%d\n", x);
    }
}
*/
