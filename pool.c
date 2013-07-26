#include <stdlib.h>
#include <assert.h>

#include "pool.h"

struct pool *pool_create(uint32_t size) {
	struct pool *pool;

	pool = (struct pool*)calloc(1, sizeof(struct pool));

	pool_resize(pool, size);

	return pool;
}

void pool_destroy(struct pool *pool) {
	assert(pool != NULL);

	free(pool->a);
	free(pool);
}

struct pool_entry *pool_get(struct pool *pool) {
	struct pool_entry *entry;

	if (pool->size == 0)
		return NULL;

	entry = (struct pool_entry*)malloc(sizeof(struct pool_entry));
	*entry = pool->a[--pool->size];

	return entry;
}

bool pool_add(struct pool *pool, struct pool_entry *entry) {
	if (pool->limit < pool->size + 1)
		return false;

	pool->a[pool->size++] = *entry;

	return true;
}

void pool_resize(struct pool *pool, uint32_t limit) {
  pool->a = (struct pool_entry*)realloc(pool->a,
			sizeof(struct pool_entry) * limit);

	pool->limit = limit;

	if (pool->size > pool->limit)
		pool->size = pool->limit;
}
