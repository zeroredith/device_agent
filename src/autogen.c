#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct String String;
typedef struct StringBuilder StringBuilder;
typedef struct Arena Arena;

#include "base.c"

#define SB_SIZE 20000

typedef enum {
    UNKNOWN,
    STRUCT,
    UNION,
    ENUM,
    FUNCTION,
    LANE_JUMP,
    COMMENT_STARTS,
    COMMENT_ENDS,
    COMMENT,
    TYPEDEF,
} TokenId;

typedef struct {
    TokenId id;
    String str;
} Token;

DA_DEFINE(int_array, u32);
DA_DEFINE(ArrayToken, Token);
DA_DEFINE(ArrayString, String);

u32 line = 0;
ArrayToken tokens;
ArrayString functions_def;
ArrayString structs_def;
ArrayString unions_def;
ArrayString enums_def;
Arena* sb_arena;
Arena* global_arena;

void print_token_id(TokenId tk_id) {
	switch(tk_id) {
		case UNKNOWN: printf("UNKNOWN"); break;
		case STRUCT: printf("STRUCT"); break;
		case UNION: printf("UNION"); break;
		case ENUM: printf("ENUM"); break;
		case FUNCTION: printf("FUNCTION"); break;
		case LANE_JUMP: printf("LANE_JUMP"); break;
		case COMMENT_STARTS: printf("COMMENT_STARTS"); break;
		case COMMENT_ENDS: printf("COMMENT_ENDS"); break;
		case COMMENT: printf("COMMENT"); break;
		case TYPEDEF: printf("TYPEDEF"); break;
	}
}


String
read_file(char* file) {
	FILE *f = fopen(file, "rb");
	if(!f) return(String){ .data = "", .len = 0 };

	StringBuilder sb = sb_init(sb_arena, 1000);
  char buffer[4096];
  s32 n;

  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    // agregar n bytes al builder
    sb_append(&sb, buffer, (s32)n);
  }
  fclose(f);
  return sb.str;
}

Token
next_token(s32* index, String* file) {
	StringBuilder sb = sb_init(sb_arena, SB_SIZE);
	for (;;) {
		(*index)++;
		if (*index >= file->len) return (Token){UNKNOWN, ""};

		char c = file->data[*index];

		if (c == ' ') break;
		if (c == '{') break;
		if (c == '\n') {
			if (sb.str.len == 0) return (Token){LANE_JUMP, 0};
			else {
				(*index)--;
				break;
			}
		}

		sb_append_char(&sb, file->data[*index]);
	}

	trimp(&sb.str);
	if (cstring_cmp(&sb.str, "//"))	return (Token){COMMENT, null};
	if (cstring_cmp(&sb.str, "/*"))	return (Token){COMMENT_STARTS, null};
	if (cstring_cmp(&sb.str, "*/"))	return (Token){COMMENT_ENDS, null};
	if (cstring_cmp(&sb.str, "struct"))	return (Token){STRUCT, null};
	if (cstring_cmp(&sb.str, "typedef"))	return (Token){TYPEDEF, null};
	if (cstring_cmp(&sb.str, "union"))	return (Token){UNION, null};
	if (cstring_cmp(&sb.str, "enum"))	return (Token){ENUM, null};
	if (cstring_cmp(&sb.str, "function"))	return (Token){FUNCTION, null};
	return (Token){UNKNOWN, sb.str};
}

String
get_function_def(s32* index, String* file) {
	StringBuilder sb = sb_init(sb_arena, SB_SIZE);
	s32 count = 0;
	for (;;) {
		char c = file->data[*index];
		if (c == '{') break;
		if (c == '\n') sb_append_char(&sb, ' ');
		else sb_append_char(&sb, c);
		count++;
		(*index)++;
	}
	trimp(&sb.str);
	sb_append_char(&sb, ';');
	sb_append_char(&sb, '\n');
	return sb.str;
}

void
jump_to_next_lane(s32* index, String* file) {
	for (;;) {
		(*index)++;
		if (*index >= file->len) return;
		if (file->data[*index] == '\n') return;
	}
}

void
generate_typedef(char* path) {
  FILE *f = fopen(path, "rb");
  String file_str = read_file(path);
	s32 line = 0;
	s32 index = 0;
	bool in_comment = false;
	bool in_comment_lane = false;
	s32 count_t = 0;

	for (;;) {
		Token token = next_token(&index, &file_str);
		da_append(&tokens, token);
		if (token.id == LANE_JUMP) line++;
		if (token.id == COMMENT) in_comment_lane = true;
		if (token.id == COMMENT_STARTS) in_comment = true;
		if (token.id == COMMENT_ENDS) in_comment = false;
		if (in_comment_lane) {
			if (token.id == LANE_JUMP) in_comment_lane = false;
			continue;
		}
		if (in_comment) continue;

		StringBuilder sb = sb_init(sb_arena, SB_SIZE);
		Token name;
		String def;
		switch(token.id) {
			case TYPEDEF:
				jump_to_next_lane(&index, &file_str);
				break;
			case STRUCT:
				name = next_token(&index, &file_str);
				if (name.id == LANE_JUMP) continue;
				da_append(&tokens, name);

				sb_append(&sb, "typedef struct ", 15);
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ' ');
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ';');
				sb_append_char(&sb, '\n');

				def = string_copy(sb.str, global_arena);
				da_append(&structs_def, def);
				break;
			case ENUM:
				name = next_token(&index, &file_str);
				if (name.id == LANE_JUMP) continue;
				da_append(&tokens, name);

				sb_append(&sb, "typedef enum ", 13);
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ' ');
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ';');
				sb_append_char(&sb, '\n');

				def = string_copy(sb.str, global_arena);
				da_append(&enums_def, def);
				break;
			case UNION:
				name = next_token(&index, &file_str);
				if (name.id == LANE_JUMP) continue;
				da_append(&tokens, name);

				sb_append(&sb, "typedef union ", 14);
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ' ');
				sb_append(&sb, name.str.data, name.str.len);
				sb_append_char(&sb, ';');
				sb_append_char(&sb, '\n');

				def = string_copy(sb.str, global_arena);
				da_append(&unions_def, def);
				break;
			case FUNCTION:
				String tmp_def = get_function_def(&index, &file_str);
				def = string_copy(tmp_def, global_arena);
				da_append(&functions_def, def);
				break;
		}
		if (index >= file_str.len) break;

	}
	arena_reset(sb_arena);
}

void
write_string(FILE *f, String s) {
	if(!f || !s.data) return;
	fprintf(f, "%.*s", (int)s.len, s.data);
}

void
write_cstring(FILE *f, char* s) {
	if(!f) return;
	fprintf(f, "%.*s", strlen(s), s);
}

int main(void) {
    FILE *f = fopen("typedefgen.h", "w");
    da_init(&tokens);
    da_init(&functions_def);
    da_init(&structs_def);
    da_init(&unions_def);
    da_init(&enums_def);

		sb_arena = arena_new(1024*1024*10);
		global_arena = arena_new(1024*1024*10);
		StringBuilder sb = sb_init(sb_arena, 1024);
    da_append(&functions_def, sb.str);
		generate_typedef("main.c");
		generate_typedef("base.c");

		// write_cstring(f, "#ifndef TYPEDEFGEN_H\n#define TYPEDEFGEN_H\n");

		write_cstring(f, "\n#define function\n");
		write_cstring(f, "\n#include \"types.h\"\n");

		write_cstring(f, "\n// STRUCT DEFS \n");
		for (s32 i = 0; i < structs_def.len; i++) write_string(f, *da_get(&structs_def, i));
		write_cstring(f, "\n// UNION DEFS \n");
		for (s32 i = 0; i < unions_def.len; i++) write_string(f, *da_get(&unions_def, i));
		write_cstring(f, "\n// ENUM DEFS \n");
		for (s32 i = 0; i < enums_def.len; i++) write_string(f, *da_get(&enums_def, i));
		write_cstring(f, "\n// FUNCTION DEFS \n");
		for (s32 i = 0; i < functions_def.len; i++) write_string(f, *da_get(&functions_def, i));

    fclose(f);
    return 0;
}
