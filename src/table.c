//
// Hey this is a mess right now, but we're experimenting with a _Generic interface for a single header hash table.
// Probably string as key is the only thing we're ever gonna use we can simplify a lot with that and use the jonathan blow C++ hash table algorithm implementation
// He elaborates on that VOD where he makes a parallel between hash tables and the birthday problem
//

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef const char *ConstCharPtr;

#define Table_Header(K, V)                                                                                                                                                                                                                                                                                                     \
   struct {                                                                                                                                                                                                                                                                                                                    \
      size_t cap, size;                                                                                                                                                                                                                                                                                                        \
      struct {                                                                                                                                                                                                                                                                                                                 \
         K key;                                                                                                                                                                                                                                                                                                                \
         V val;                                                                                                                                                                                                                                                                                                                \
         uint64_t hash;                                                                                                                                                                                                                                                                                                        \
      } *entries;                                                                                                                                                                                                                                                                                                              \
   }

struct generic_table_header {
   size_t cap, size;
   void *entries;
};

static inline uint64_t hash_data(const void *data, size_t len) {
   uint64_t h = 14695981039346656037ULL;
   for (size_t i = 0; i < len; i++)
      h = (h ^ ((uint8_t *)data)[i]) * 1099511628211ULL;
   return h ? h : 1; // ensure non-zero
}

static inline size_t find(void *table, const void *key, size_t key_size, size_t entry_size, uint64_t key_hash) {
   struct generic_table_header *t = (struct generic_table_header *)table;

   for (size_t i = 0, pos = key_hash % t->cap; i < t->cap; i++, pos = (key_hash + i * i) % t->cap) {
      uint64_t *entry_hash = (uint64_t *)((uint8_t *)t->entries + pos * entry_size + entry_size - 8);
      if (!*entry_hash)
         return pos; // empty slot
      if (*entry_hash == key_hash && !memcmp((uint8_t *)t->entries + pos * entry_size, key, key_size))
         return pos; // found
   }
   return t->cap; // full
}

static inline void grow(void *table, size_t key_size, size_t entry_size) {
   struct generic_table_header *t = (struct generic_table_header *)table;
   if (t->size * 4 <= t->cap * 3)
      return;

   void *old = t->entries;
   size_t old_cap = t->cap;
   t->cap *= 2;
   t->size = 0;
   t->entries = calloc(t->cap, entry_size);

   for (size_t i = 0; i < old_cap; i++) {
      uint8_t *old_entry = (uint8_t *)old + i * entry_size;
      uint64_t *old_hash = (uint64_t *)(old_entry + entry_size - 8);

      if (*old_hash) {
         size_t slot = find(table, old_entry, key_size, entry_size, *old_hash);
         memcpy((uint8_t *)t->entries + slot * entry_size, old_entry, entry_size);
         t->size++;
      }
   }
   free(old);
}

#define init(t, n)                                                                                                                                                                                                                                                                                                             \
   do {                                                                                                                                                                                                                                                                                                                        \
      (t)->cap = n < 8 ? 8 : n;                                                                                                                                                                                                                                                                                                \
      (t)->size = 0;                                                                                                                                                                                                                                                                                                           \
      (t)->entries = calloc((t)->cap, sizeof(*(t)->entries));                                                                                                                                                                                                                                                                  \
   } while (0)

#define set(t, k, v)                                                                                                                                                                                                                                                                                                           \
   do {                                                                                                                                                                                                                                                                                                                        \
      size_t key_sz = _Generic(k, default: sizeof(k), ConstCharPtr: strlen(k), char *: strlen(k));                                                                                                                                                                                                                             \
      uint64_t h = hash_data(_Generic(k, default: &k, ConstCharPtr: k, char *: k), key_sz);                                                                                                                                                                                                                                    \
      grow(t, key_sz, sizeof(*(t)->entries));                                                                                                                                                                                                                                                                                  \
      size_t i = find(t, _Generic(k, default: &k, ConstCharPtr: k, char *: k), key_sz, sizeof(*(t)->entries), h);                                                                                                                                                                                                              \
      if (i < (t)->cap) {                                                                                                                                                                                                                                                                                                      \
         if (!(t)->entries[i].hash)                                                                                                                                                                                                                                                                                            \
            (t)->size++;                                                                                                                                                                                                                                                                                                       \
         (t)->entries[i].key = k;                                                                                                                                                                                                                                                                                              \
         (t)->entries[i].val = v;                                                                                                                                                                                                                                                                                              \
         (t)->entries[i].hash = h;                                                                                                                                                                                                                                                                                             \
      }                                                                                                                                                                                                                                                                                                                        \
   } while (0)

static inline void *table_get_entry(void *table, const void *key, size_t key_size, size_t entry_size, uint64_t key_hash) {
   struct generic_table_header *t = (struct generic_table_header *)table;
   size_t i = find(t, key, key_size, entry_size, key_hash);

   if (i < t->cap) {
      uint64_t *entry_hash = (uint64_t *)((uint8_t *)t->entries + i * entry_size + entry_size - 8);
      if (*entry_hash)
         return (uint8_t *)t->entries + i * entry_size + key_size;
   }
   return NULL;
}

#define get(t, k)                                                                                                                                                                                                                                                                                                              \
   ((typeof(&(t)->entries[0].val))table_get_entry(t, _Generic(k, ConstCharPtr: k, char *: k, default: &k), _Generic(k, ConstCharPtr: strlen(k), char *: strlen(k), default: sizeof(k)), sizeof(*(t)->entries),                                                                                                                 \
                                                  hash_data(_Generic(k, ConstCharPtr: k, char *: k, default: &k), _Generic(k, ConstCharPtr: strlen(k), char *: strlen(k), default: sizeof(k)))))

#define del(t, k)                                                                                                                                                                                                                                                                                                              \
   do {                                                                                                                                                                                                                                                                                                                        \
      size_t key_sz = _Generic(k, ConstCharPtr: strlen(k), char *: strlen(k), default: sizeof(k));                                                                                                                                                                                                                             \
      uint64_t h = hash_data(_Generic(k, default: &k, ConstCharPtr: k, char *: k), key_sz);                                                                                                                                                                                                                                    \
      size_t i = find(t, _Generic(k, default: &k, ConstCharPtr: k, char *: k), key_sz, sizeof(*(t)->entries), h);                                                                                                                                                                                                              \
      if (i < (t)->cap && (t)->entries[i].hash) {                                                                                                                                                                                                                                                                              \
         (t)->entries[i].hash = 0;                                                                                                                                                                                                                                                                                             \
         (t)->size--;                                                                                                                                                                                                                                                                                                          \
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
