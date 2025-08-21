#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef const char *ConstCharPtr; // Allow tcc to compile it (too much stuff under _Gerenic breaks tcc)

#define Table_Header(K, V)                                                                                                                                                                                                                                                                                                     \
   struct {                                                                                                                                                                                                                                                                                                                    \
      size_t capacity, count;                                                                                                                                                                                                                                                                                                        \
      struct {                                                                                                                                                                                                                                                                                                                 \
         K key;                                                                                                                                                                                                                                                                                                                \
         V val;                                                                                                                                                                                                                                                                                                                \
         uint8_t state;                                                                                                                                                                                                                                                                                                        \
      } *entries;                                                                                                                                                                                                                                                                                                              \
   }


struct generic_table_header {
   size_t capacity, count;
   void *entries;
};

static inline uint64_t hash(const void *data, size_t len) {
   uint64_t h = 14695981039346656037ULL;
   for (size_t i = 0; i < len; i++) {
      h = (h ^ ((uint8_t *)data)[i]) * 1099511628211ULL;
   }
   return h;
}

static inline size_t find(void *table, const void *key, size_t key_size, size_t entry_size) {
   struct generic_table_header *t = (struct generic_table_header *)table;
   uint64_t h = hash(key, key_size);
   for (size_t i = h % t->capacity, j = 0; j < t->capacity; i = (i + 1) % t->capacity, j++) {
      uint8_t *entry = (uint8_t *)t->entries + i * entry_size;
      if (!entry[entry_size - 1]) {
         return i; // empty
      }
      if (entry[entry_size - 1] == 1 && !memcmp(entry, key, key_size))
         return i; // found
   }
   return t->capacity; // full
}

static inline void grow(void *table, size_t key_size, size_t entry_size) {
   struct generic_table_header *t = (struct generic_table_header *)table;
   if (t->count * 4 <= t->capacity * 3)
      return;

   void *old = t->entries;
   size_t old_capacity = t->capacity;
   t->capacity *= 2;
   t->count = 0;
   t->entries = calloc(t->capacity, entry_size);

   for (size_t i = 0; i < old_capacity; i++) {
      uint8_t *old_entry = (uint8_t *)old + i * entry_size;
      if (old_entry[entry_size - 1] == 1) {
         size_t slot = find(table, old_entry, key_size, entry_size);
         memcpy((uint8_t *)t->entries + slot * entry_size, old_entry, entry_size);
         t->count++;
      }
   }
   free(old);
}

#define init(t, n)                                                                                                                                                                                                                                                                                                             \
   do {                                                                                                                                                                                                                                                                                                                        \
      (t)->capacity = n < 8 ? 8 : n;                                                                                                                                                                                                                                                                                                \
      (t)->count = 0;                                                                                                                                                                                                                                                                                                           \
      (t)->entries = calloc((t)->capacity, sizeof(*(t)->entries));                                                                                                                                                                                                                                                                  \
   } while (0)

#define set(t, k, v)                                                                                          \
   do {                                                                                                       \
      size_t key_sz =    _Generic(k, default: sizeof(k), ConstCharPtr: strlen(k), char *: strlen(k));         \
      grow(t, key_sz, sizeof(*(t)->entries));                                                                 \
      size_t i = find(t, _Generic(k, default: &k, ConstCharPtr: k, char *: k), key_sz, sizeof(*(t)->entries));\
      if (i < (t)->capacity) {                                                                                \
         if (!(t)->entries[i].state)                                                                          \
            (t)->count++;                                                                                     \
         (t)->entries[i].key = k;                                                                             \
         (t)->entries[i].val = v;                                                                             \
         (t)->entries[i].state = 1;                                                                           \
      }                                                                                                       \
   } while (0)

#define get(t, k)                                                                                                                                                                                                                                                                                                              \
   ({                                                                                                             \
      typeof(k) _k = k;                                                                                           \
      size_t key_sz = _Generic(_k, ConstCharPtr: strlen(_k), char *: strlen(_k), default: sizeof(_k));            \
      size_t i = find(t, _Generic(_k, default: &_k, ConstCharPtr: _k, char *: _k), key_sz, sizeof(*(t)->entries));\
      (i < (t)->capacity && (t)->entries[i].state) ? &(t)->entries[i].val : (typeof(&(t)->entries[i].val))0;           \
   })

#define del(t, k)                                                                                                                                                                                                                                                                                                              \
   do {                                                                                                                                                                                                                                                                                                                        \
      typeof(k) _k = k;                                                                                                                                                                                                                                                                                                        \
      size_t key_sz = _Generic(_k, ConstCharPtr: strlen(_k), char *: strlen(_k), default: sizeof(_k));                                                                                                                                                                                                                              \
      size_t i = find(t, _Generic(_k, default: &_k, ConstCharPtr: _k, char *: _k), key_sz, sizeof(*(t)->entries));                                                                                                                                                                                                                  \
      if (i < (t)->capacity && (t)->entries[i].state) {                                                                                                                                                                                                                                                                             \
         (t)->entries[i].state = 0;                                                                                                                                                                                                                                                                                            \
         (t)->count--;                                                                                                                                                                                                                                                                                                          \
      }                                                                                                                                                                                                                                                                                                                        \
   } while (0)


struct String_Table {
   Table_Header(ConstCharPtr, int);
   int other_values_that_doesnt_matter[10];
};

void example() {
   struct String_Table t;
   init(&t, 16);
   set(&t, "hello", 42);
   int *val = get(&t, "hello");
   printf("val = %d\n", *val);
   del(&t, "hello");
   val = get(&t, "hello");
   printf("val = %p\n", val);
   free(t.entries);
}

int main(int argc, char *argv[]) {
   example();
   return EXIT_SUCCESS;
}
