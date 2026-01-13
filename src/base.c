#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "types.h"

#define DA_DEFINE(name, type) \
    typedef struct { \
        type *data; \
        size_t len; \
        size_t capacity; \
    } name

#define da_init(xp) \
    do { \
        (xp)->data = NULL; \
        (xp)->len = 0; \
        (xp)->capacity = 0; \
    } while (0)

// #define da_append(xs, x) \
//     do { \
//         if ((xs)->len >= (xs)->capacity) { \
//             if ((xs)->capacity == 0) (xs)->capacity = 256; \
//             else (xs)->capacity *= 2; \
//             (xs)->data = realloc((xs)->data, (xs)->capacity * sizeof(*(xs)->data)); \
//         } \
//         (xs)->data[(xs)->len++] = (x); \
//     } while (0)

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
} __packed;

struct StringBuilder
{
	String str;
	Arena* arena;
	u64 capacity;
} __packed;

struct Arena
{
	u64 used;
	u64 size;
	u8* data;
	Arena* next;
} __packed;


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

function internal inline StringBuilder
sb_init(Arena* arena, u64 initial_capacity)
{
  StringBuilder sb;
  sb.arena = arena;
  sb.str.data = arena_alloc(arena, initial_capacity);
  sb.str.len = 0;
  sb.capacity = initial_capacity;
  return sb;
}

function internal inline void
sb_reset(StringBuilder* sb)
{
	if(sb) sb->str.len = 0;
}

function internal inline void
sb_append(StringBuilder* sb, char* data, u64 len)
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
sb_append_char(StringBuilder* sb, char c)
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
sb_pop(StringBuilder* sb)
{
    if(sb->str.len > 0) sb->str.len--;
}

function internal inline bool
sb_read_line(StringBuilder* sb, FILE* f, u64* line_count)
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

		if (!sb_append_char(sb, (char)c)) return false;
	}

	// Devolver true si leímos algo (incluso si fue solo \n)
	return line_read || (c != EOF);
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
	if (str.len > 0 && str.data[str.len] == '\0') return str;

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

function internal inline String
string_copy_null_terminated(String src, Arena* arena)
{
	String result = {0};
	if (!arena) return result;

  u8 needs_null = (src.data[src.len-1] != '\0');

  result.data = arena_alloc(arena, src.len + needs_null);
  if (!result.data) return result;

  memcpy(result.data, src.data, src.len);
  result.len = src.len;

  if (needs_null) result.data[result.len] = '\0';
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
	return (str->data[str->len-1] == '\0');
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

function internal inline String
trim(String s, Arena* arena)
{
	if(s.len == 0) return s;
	u64 count = 0;
	StringBuilder new_s = sb_init(arena, DEFAULT_SIZE);
	while(isspace(s.data[count])) count++;
	for(u64 i=count; i < s.len; i++)
	{
		new_s.str.data[new_s.str.len++] = s.data[i];
	}

	if(new_s.str.len == 0) return new_s.str;

	while(isspace(new_s.str.data[new_s.str.len-1])) new_s.str.len--;

	return new_s.str;
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

function internal inline bool
split(String s, String* out_left, String* out_right, char separator)
{
	assert(out_left && out_right);

	*out_left = (String){0};
	*out_right = (String){0};

	u64 split_pos = 0;
	bool found = false;

	for (u64 i = 0; i < s.len; i++)
	{
		if (s.data[i] == separator)
		{
			split_pos = i;
			found = true;
			break;
		}
	}

	if (!found) return false;

	out_left->data = s.data;
	out_left->len = split_pos;

	if (split_pos + 1 < s.len)
	{
		out_right->data = s.data + split_pos + 1;
		out_right->len = s.len - split_pos - 1;
	} else
	{
		out_right->data = s.data + s.len;
		out_right->len = 0;
	}

	return true;
}

function internal inline String
remove_extra_spaces(String str, Arena* arena)
{
	StringBuilder sb = sb_init(arena, DEFAULT_SIZE);
	for(u32 i = 0; i < str.len; i++)
	{
		if(isspace(str.data[i]) && i != str.len-1)
		{
			if(!isspace(str.data[i+1])) sb_append_char(&sb, str.data[i]);
		}
		else sb_append_char(&sb, str.data[i]);
	}
	return sb.str;
}

function internal inline u64
count_words(String str, Arena* arena)
{
	String trimmed_str = remove_extra_spaces(trim(str, arena), arena);
	u64 words = 0;

	if(trimmed_str.len == 0) return 0;
	for(u64 i = 0; i < trimmed_str.len; i++) if(isspace(trimmed_str.data[i])) words++;

	return words + 1;
}

function internal inline u64
count_letters(String str)
{
	u64 letters = 0;
	for(u64 i = 0; i < str.len; i++) if(!isspace(str.data[i])) letters++;
	return letters;
}

function internal inline void
itoa(s64 n, String s) {
	sprintf(s.data, "%d", n);
}


#define char_to_s32(c) ((s32)((c) - '0'))