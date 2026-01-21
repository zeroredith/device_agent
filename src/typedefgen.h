#ifndef TYPEDEFGEN_H
#define TYPEDEFGEN_H

#define function

#include "types.h" 

// 
// STRUCT DEFS
// 
typedef struct String String;
typedef struct String_Builder_Arena String_Builder_Arena;
typedef struct String_Builder String_Builder;
typedef struct Arena Arena;
typedef struct dirent dirent;
typedef struct dirent dirent;

// 
// ENUM DEFS
// 

// 
// UNION DEFS
// 

// 
// FUNCTION DEFS
// 
internal inline Arena* arena_new(u64 size);
internal inline void arena_reset(Arena* arena);
internal inline void arena_free(Arena* arena);
internal inline void* arena_alloc(Arena* arena, u64 size);
internal inline String_Builder sb_init(u64 initial_capacity);
internal inline String sb_to_string(String_Builder* sb);
internal inline void sb_append(String_Builder* sb, String s);
internal inline void sb_append_char(String_Builder* sb, char c);
internal inline String_Builder_Arena sb_init_arena(Arena* arena, u64 initial_capacity);
internal inline void sb_reset(String_Builder_Arena* sb);
internal inline void sb_arena_append(String_Builder_Arena* sb, char* data, u64 len);
internal inline bool sb_arena_append_char(String_Builder_Arena* sb, char c);
internal inline void sb_pop(String_Builder_Arena* sb);
internal inline bool sb_read_line(String_Builder_Arena* sb, FILE* f, u64* line_count);
internal inline String string_reserve(u64 size);
internal inline String string_arena(Arena* arena, char* str);
internal inline String string_arena_init(Arena* arena, u64 size);
internal inline String string_to_string(Arena* arena, String* str);
internal inline String string_to_cstr(String str, Arena* arena);
internal inline String string_copy(String src, Arena* arena);
internal inline bool string_copy_to(String src, String* dest, Arena* arena);
internal inline bool is_cstr(String* str);
internal inline void to_cstr(String* str);
internal inline void to_string(String* str);
internal inline bool string_cmp(String* s1, String* s2);
internal inline bool cstring_cmp(String* s1, char* s2);
internal inline void trimlp(String* s);
internal inline void trimrp(String* s);
internal inline void trimp(String* s);
internal inline void prints(String s);
internal inline void printsln(String s);

#endif
