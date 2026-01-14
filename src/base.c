#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "types.h"

#define da_init(xp) \
    do { \
        (xp)->data = NULL; \
        (xp)->len = 0; \
        (xp)->capacity = 0; \
    } while (0)

#define da_append(xs, x) ({                                                     \
    if ((xs)->len >= (xs)->capacity) {                                          \
        if ((xs)->capacity == 0) (xs)->capacity = 256;                          \
        else (xs)->capacity *= 2;                                               \
        (xs)->data = realloc((xs)->data, (xs)->capacity * sizeof(*(xs)->data)); \
    }                                                                           \
    (xs)->data[(xs)->len] = (x);                                                \
    &((xs)->data[(xs)->len++]);                                                 \
})


#define da_free(xp) \
    do { \
        free((xp)->data); \
        (xp)->data = NULL; \
        (xp)->len = 0; \
        (xp)->capacity = 0; \
    } while (0)

#define da_get(xs, i) (&(xs)->data[(i)])
#define da_set(xs, x, i) \
		do { \
      if ((xs)->len >= (xs)->capacity) { \
          if ((xs)->capacity == 0) (xs)->capacity = 256; \
          else (xs)->capacity *= 2; \
          (xs)->data = realloc((xs)->data, (xs)->capacity * sizeof(*(xs)->data)); \
      } \
		  (xs)->data[(i)] = (x); \
		  (xs)->len++; \
		} while(0)


struct String
{
  u8* data;
  u64 len;
};

struct String_Builder_Arena
{
	String str;
	Arena* arena;
	u64 capacity;
};

struct String_Builder
{
	u8* data;
	u64 len;
	u64 capacity;
};

struct Arena
{
	u64 used;
	u64 size;
	u8* data;
	Arena* next;
};

//
//	Arena
//

function internal inline Arena*
arena_new(u64 size) {
	Arena* arena = (Arena*)malloc(sizeof(Arena));
  if (!arena) return null;

	arena->data = (u8*)malloc(size);
	if(!arena->data)
	{
		free(arena);
		return null;
	}
	arena->size = size;
	arena->used = 0;
	arena->next = null;
	return arena;
}

function internal inline void
arena_reset(Arena* arena)
{
	while(arena)
	{
		arena->used = 0;
		arena = arena->next;
	}
}

function internal inline void
arena_free(Arena* arena)
{
	while(arena)
	{
		Arena* next = arena->next;
		if (arena->data) free(arena->data);
		free(arena);
		arena = next;
	}
}

// The arena have to be enough big to be able to hold at least the biggest object you gonna put
function internal inline void*
arena_alloc(Arena* arena, u64 size)
{
	if (size == 0) return null;
	assert(size <= arena->size);
	while(arena)
	{
		if (arena->used + size <= arena->size)
		{
			void* ptr = arena->data + arena->used;
			arena->used += size;
			return ptr;
		}

		if (!arena->next)
		{
			arena->next = arena_new(size);
			if (!arena->next) return null;
		}

		arena = arena->next;
	}
	return null;
}


//
// STRING
//

function internal inline String_Builder
sb_init(u64 initial_capacity) {
  if (initial_capacity == 0) initial_capacity = 1;
  String_Builder sb;
  sb.data = malloc(initial_capacity);
  sb.len = 0;
  sb.capacity = initial_capacity;
  return sb;
}

function internal inline String
sb_to_string(String_Builder* sb) {
	return (String){sb->data, sb->len};
}

function internal inline void
sb_append(String_Builder* sb, String s) {
	if (sb->capacity - sb->len + s.len > sb->capacity) {
		u8* new_data = malloc(sb->capacity*2);
		memcpy(new_data, sb->data, sb->len);
		free(sb->data);
		sb->data = new_data;
	}

	memcpy(sb->data + sb->len, s.data, s.len);
	sb->len += s.len;
}

function internal inline void
sb_append_char(String_Builder* sb, char c) {
	if (sb->capacity - sb->len + 1 > sb->capacity) {
		u8* new_data = malloc(sb->capacity*2);
		memcpy(new_data, sb->data, sb->len);
		free(sb->data);
		sb->data = new_data;
	}

	sb->data[sb->len] = c;
	sb->len += 1;
}

function internal inline String_Builder_Arena
sb_init_arena(Arena* arena, u64 initial_capacity)
{
  if (initial_capacity == 0) initial_capacity = 1;
  String_Builder_Arena sb;
  sb.arena = arena;
  sb.str.data = arena_alloc(arena, initial_capacity);
  sb.str.len = 0;
  sb.capacity = initial_capacity;
  return sb;
}

function internal inline void
sb_reset(String_Builder_Arena* sb)
{
	if(sb) sb->str.len = 0;
}

function internal inline void
sb_arena_append(String_Builder_Arena* sb, char* data, u64 len)
{
	if (sb->str.len + len >= sb->capacity) {
		u64 new_capacity = sb->capacity * 2;
		while (sb->str.len + len >= new_capacity) {
			new_capacity *= 2;
		}

		u8* new_data = arena_alloc(sb->arena, new_capacity);
		memcpy(new_data, sb->str.data, sb->str.len);
		sb->str.data = new_data;
		sb->capacity = new_capacity;
	}

	memcpy(sb->str.data + sb->str.len, data, len);
	sb->str.len += len;
}

function internal inline bool
sb_arena_append_char(String_Builder_Arena* sb, char c)
{
    if (!sb) return false;

    if (sb->str.len >= sb->capacity)
    {
        u64 new_capacity = sb->capacity * 2;
        if (new_capacity < 16) new_capacity = 16;

        u8* new_data = arena_alloc(sb->arena, new_capacity);
        if (!new_data) return false;

        if (sb->str.len > 0 && sb->str.data) memcpy(new_data, sb->str.data, sb->str.len);

        sb->str.data = new_data;
        sb->capacity = new_capacity;
    }

    sb->str.data[sb->str.len++] = (u8)c;
    return true;
}

function internal inline void
sb_pop(String_Builder_Arena* sb)
{
    if(sb->str.len > 0) sb->str.len--;
}

function internal inline bool
sb_read_line(String_Builder_Arena* sb, FILE* f, u64* line_count)
{
	char buffer[DEFAULT_SIZE];
	if (!sb || !f) return false;
  sb_reset(sb);

	s32 c;
	bool line_read = false;
	while ((c = fgetc(f)) != EOF)
	{
		line_read = true;

		if (c == '\n')
		{
			if (line_count) (*line_count)++;
			break;
		}

		if (c == '\r') continue;

		if (!sb_arena_append_char(sb, (char)c)) return false;
	}

	// Devolver true si leímos algo (incluso si fue solo \n)
	return line_read || (c != EOF);
}

#define string_init(str) (String){.data = str, .len = strlen(str)}

function internal inline String
string_reserve(u64 size){
	assert(size > 0);
	String s = {0};
	s.len = size;
	s.data = malloc(size+1);
	return s;
}

function internal inline String
string_arena(Arena* arena, char* str) {
	String result = {0};
	u64 len = strlen(str);
	result.data = arena_alloc(arena, len);
	if (!result.data) return result;
	result.len = len;
	if (str)	memcpy(result.data, str, len);
	return result;
}

function internal inline String
string_arena_init(Arena* arena, u64 size) {
	String result = {0};
	result.data = arena_alloc(arena, size);
	if (!result.data) return result;
	result.len = size;

	return result;
}

function internal inline String
string_to_string(Arena* arena, String* str) {
	String result = {0};
	result.data = arena_alloc(arena, str->len);
	if (!result.data) return result;
	result.len = str->len;
	if (str) memcpy(result.data, str->data, str->len);
	return result;
}

String
string_from_cstr(Arena* arena, char* cstr)
{
	u64 len = strlen(cstr);
	return string_arena(arena, cstr);
}

function internal inline String
string_to_cstr(String str, Arena* arena)
{
	if (str.len > 0 && str.data[str.len - 1] == '\0') return str;

	String result = {
		.data = arena_alloc(arena, str.len+1),
		.len = str.len
	};
	if(!result.data) return(String){0};

	if(str.len > 0 && str.data) memcpy(result.data, str.data, str.len);
	result.data[str.len] = '\0';

	return result;
}

function internal inline String
string_copy(String src, Arena* arena)
{
	String result = {0};
	if (!arena) return result;

	result.data = arena_alloc(arena, src.len);
	if (!result.data) return result;

	memcpy(result.data, src.data, src.len);
	result.len = src.len;
	return result;
}

function internal inline bool
string_copy_to(String src, String* dest, Arena* arena)
{
	if (!dest || !arena) return false;

	if (dest->data && dest->len >= src.len)
	{
		memcpy(dest->data, src.data, src.len);
		dest->len = src.len;
		return true;
	}

	dest->data = arena_alloc(arena, src.len);
	if (!dest->data) return false;

	memcpy(dest->data, src.data, src.len);
	dest->len = src.len;
	return true;
}

function internal inline bool
is_cstr(String* str)
{
	if (!str || !str->data || str->len == 0) return false;
	return (str->data[str->len - 1] == '\0');
}

function internal inline void
to_cstr(String* str)
{
	if(str->data[str->len] != '\0') str->data[str->len] = '\0';
}

function internal inline void
to_string(String* str)
{
	if (!str || !str->data || str->len == 0) return;
	if (str->data[str->len - 1] == '\0') str->len--;
}

function internal inline bool
string_cmp(String* s1, String* s2)
{
	if(memcmp(s1->data, s2->data, s1->len) == 0) return 1;
	else return 0;
}

function internal inline bool
cstring_cmp(String* s1, char* s2)
{
	if(memcmp(s1->data, s2, strlen(s2)) == 0) return 1;
	else return 0;
}

function internal inline void
trimlp(String* s)
{
	if (!s || !s->data || s->len == 0) return;
	u64 count = 0;

	while (count < s->len && isspace(s->data[count])) count++;

	if (count == 0) return;

	memmove(s->data, s->data + count, s->len - count);
	s->len -= count;
}

function internal inline void
trimrp(String* s)
{
	if (!s || !s->data || s->len == 0) return;

	if (s->len > 0 && s->data[s->len - 1] == '\0') s->len--;

	u64 last_char = s->len;
	while (last_char > 0 && isspace(s->data[last_char - 1])) last_char--;

	s->len = last_char;
}

function internal inline void
trimp(String* s)
{
	trimlp(s);
	trimrp(s);
}

function internal inline void
prints(String s)
{
	printf("%.*s", (int)s.len, s.data);
}

function internal inline void
printsln(String s)
{
	printf("%.*s\n", (int)s.len, s.data);
}


// function internal inline void
// itoa(s64 n, String s) {
// 	sprintf(s.data, "%d", n);
// }


#define char_to_s32(c) ((s32)((c) - '0'))