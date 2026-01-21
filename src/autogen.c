#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define STB_DS_IMPLEMENTATION
#define STB_C_LEXER_IMPLEMENTATION
#include "stb_ds.h"
#include "stb_c_lexer.h"

// This is used to differenciate between pointers (even pointers used for arrays) and stb_ds arrays
// which have a hidden header with the len and capacity of the array.
#define Array(type) type*

typedef struct String String;
typedef struct String_Builder_Arena String_Builder_Arena;
typedef struct String_Builder String_Builder;
typedef struct Arena Arena;

#include "base.c"

Array(String) function_defs;
Array(String) enum_defs;
Array(String) struct_defs;
Array(String) union_defs;

typedef enum Def_Type {
  STRUCT,
  UNION,
  ENUM
} Def_Type;

void
add_declaration(Array(String)* defs, String name, Def_Type type) {
  if (cstring_cmp(&name, "{")) return;
  String_Builder sb = sb_init(64);
  sb_append(&sb, string_init("typedef "));

  switch(type) {
  case STRUCT: sb_append(&sb, string_init("struct ")); break;
  case ENUM:   sb_append(&sb, string_init("enum ")); break;
  case UNION:  sb_append(&sb, string_init("union ")); break;
  }

  sb_append(&sb, name);
  sb_append_char(&sb, ' ');
  sb_append(&sb, name);
  sb_append_char(&sb, ';');
  arrpush(*defs, sb_to_string(&sb));
}

void
write_string(FILE *f, String s) {
	if(!f || !s.data) return;
	fprintf(f, "%.*s", (int)s.len, s.data);
}

void
parse_file(char* file_name) {
  FILE* f = fopen(file_name, "r");
  if(!f) {
    perror("fopen");
    return;
  }

  fseek(f, 0, SEEK_END);
  u64 size = ftell(f);
  fseek(f, 0, SEEK_SET);

  String file_buffer = string_reserve(size+1);
  fread(file_buffer.data, 1, size, f);
  file_buffer.data[size] = '\0';
  fclose(f);

  stb_lexer lexer;
  char buffer_token[1024];

  stb_c_lexer_init(&lexer, file_buffer.data, file_buffer.data + file_buffer.len, buffer_token, sizeof(buffer_token));
  int tok;
  bool in_function_def = false;
  bool in_struct_def = false;
  bool in_union_def = false;
  bool in_enum_def = false;

  Array(char) def_buffer = 0;

  while((tok = stb_c_lexer_get_token(&lexer)) != 0) {
    int word_size = (int)(lexer.where_lastchar - lexer.where_firstchar+1);


    if (strncmp(lexer.where_firstchar, "{", 1) == 0 && in_function_def) {
      arrpush(def_buffer, ';');
      in_function_def = false;
      Array(char) tmp_arr = 0;
      for (int i = 0; i < arrlen(def_buffer) -1; i++) arrpush(tmp_arr, def_buffer[i]);
      String s = {tmp_arr, arrlen(tmp_arr)};
      arrpush(function_defs, s);
      def_buffer = 0;
    }

    if (in_function_def) {
      bool no_space = false;
      for (int i = 0; i < word_size; i++) {
        char c = lexer.where_firstchar[i];

        if (c == '\0') break;
        if (c == '(' || c == ')') {
          if (def_buffer[arrlen(def_buffer) - 1] == ' ') {
            arrpop(def_buffer);
            no_space = true;
          }
        }
        if (c == ',' && def_buffer[arrlen(def_buffer) - 1] == ' ') arrpop(def_buffer);
        if (c == '*' && def_buffer[arrlen(def_buffer) - 1] == ' ') arrpop(def_buffer);
        arrpush(def_buffer, c);
      }

      if (!no_space) arrpush(def_buffer, ' ');
    }

    if (strncmp(lexer.where_firstchar, "function", 8) == 0) {
      in_function_def = true;
    }

    if (in_union_def) {
      String s = {lexer.where_firstchar, word_size};
      add_declaration(&union_defs, s, UNION);
      in_union_def = false;
    }
    else if (in_struct_def) {
      String s = {lexer.where_firstchar, word_size};
      add_declaration(&struct_defs, s, STRUCT);
      in_struct_def = false;
    }
    else if (in_union_def){
      String s = {lexer.where_firstchar, word_size};
      add_declaration(&enum_defs, s, ENUM);
      in_enum_def = false;
    }

    if (strncmp(lexer.where_firstchar, "union", 5) == 0) in_union_def = true;
    if (strncmp(lexer.where_firstchar, "enum", 4) == 0) in_enum_def = true;
    if (strncmp(lexer.where_firstchar, "struct", 6) == 0) in_struct_def = true;
  }
  free(file_buffer.data);
}


int main() {


  parse_file("../src/base.c");
  parse_file("../src/main.c");
  FILE* new_f = fopen("../src/typedefgen.h", "wb");

  write_string(new_f, string_init("#ifndef TYPEDEFGEN_H\n#define TYPEDEFGEN_H\n"));
  write_string(new_f, string_init("\n#define function\n"));
  write_string(new_f, string_init("\n#include \"types.h\" \n"));

  write_string(new_f, string_init("\n// \n"));
  write_string(new_f, string_init("// STRUCT DEFS"));
  write_string(new_f, string_init("\n// \n"));

  for (int i = 0; i < arrlen(struct_defs); i++) {
    write_string(new_f, struct_defs[i]);
    write_string(new_f, string_init("\n"));
  }
  write_string(new_f, string_init("\n// \n"));
  write_string(new_f, string_init("// ENUM DEFS"));
  write_string(new_f, string_init("\n// \n"));

  for (int i = 0; i < arrlen(enum_defs); i++) {
    write_string(new_f, enum_defs[i]);
    write_string(new_f, string_init("\n"));
  }

  write_string(new_f, string_init("\n// \n"));
  write_string(new_f, string_init("// UNION DEFS"));
  write_string(new_f, string_init("\n// \n"));

  for (int i = 0; i < arrlen(union_defs); i++) {
    write_string(new_f, union_defs[i]);
    write_string(new_f, string_init("\n"));
  }
  write_string(new_f, string_init("\n// \n"));
  write_string(new_f, string_init("// FUNCTION DEFS"));
  write_string(new_f, string_init("\n// \n"));

  for (int i = 0; i < arrlen(function_defs); i++) {
    write_string(new_f, function_defs[i]);
    write_string(new_f, string_init(";"));
    write_string(new_f, string_init("\n"));
  }

  write_string(new_f, string_init("\n#endif\n"));
  fclose(new_f);


  return 0;
}