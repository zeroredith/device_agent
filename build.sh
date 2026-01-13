#!/usr/bin/env bash
WARNINGS=""
DEBUG=""
RUN=0
GOREGEN=0
SOURCE="src/main.c"
NO_WARNINGS="-Wno-unused-function -Wno-missing-braces -Wno-unused-variable -Wno-unused-parameter"
WEB=0
AUTOGEN=1

for arg in "$@"; do
	if [ "$arg " = "-n" ]; then
		AUTOGEN=0
	fi
  if [ "$arg" = "-w" ]; then
    WARNINGS="-Wall -Wextra"
  fi
  if [ "$arg" = "-d" ]; then
    DEBUG="-DDEBUG"
  fi
done

# if [ AUTOGEN = 1 ]; then
gcc src/autogen.c -o src/autogen
# fi

mkdir -p build

cd src
./autogen
cd ..

gcc $SOURCE $WARNINGS $NO_WARNINGS $DEBUG -g3 -fsanitize=address -fshort-enums -o build/game