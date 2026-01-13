
#define function

#include "types.h"

// STRUCT DEFS 
typedef struct String String;
typedef struct StringBuilder StringBuilder;
typedef struct Arena Arena;

// UNION DEFS 

// ENUM DEFS 

// FUNCTION DEFS 
internal inline Arena* arena_new(u64 size);
internal inline void arena_reset(Arena* arena);
internal inline void arena_free(Arena* arena);
internal inline void* arena_alloc(Arena* arena, u64 size);
internal inline StringBuilder sb_init(Arena* arena, u64 initial_capacity);
internal inline void sb_reset(StringBuilder* sb);
internal inline void sb_append(StringBuilder* sb, char* data, u64 len);
internal inline bool sb_append_char(StringBuilder* sb, char c);
internal inline void sb_pop(StringBuilder* sb);
internal inline bool sb_read_line(StringBuilder* sb, FILE* f, u64* line_count);
internal inline String string_arena(Arena* arena, char* str);
internal inline String string_arena_init(Arena* arena, u64 size);
internal inline String string_to_string(Arena* arena, String* str);
internal inline String string_to_cstr(String str, Arena* arena);
internal inline String string_copy(String src, Arena* arena);
internal inline String string_copy_null_terminated(String src, Arena* arena);
internal inline bool string_copy_to(String src, String* dest, Arena* arena);
internal inline bool is_cstr(String* str);
internal inline void to_cstr(String* str);
internal inline void to_string(String* str);
internal inline bool string_cmp(String* s1, String* s2);
internal inline bool cstring_cmp(String* s1, char* s2);
internal inline String trim(String s, Arena* arena);
internal inline void trimlp(String* s);
internal inline void trimrp(String* s);
internal inline void trimp(String* s);
internal inline void prints(String s);
internal inline void printsln(String s);
internal inline bool split(String s, String* out_left, String* out_right, char separator);
internal inline String remove_extra_spaces(String str, Arena* arena);
internal inline u64 count_words(String str, Arena* arena);
internal inline u64 count_letters(String str);
internal inline void itoa(s64 n, String s);
