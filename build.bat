@echo off
setlocal enabledelayedexpansion

set WARNINGS=
set DEBUG=
set RUN=0
set GOREGEN=0
set SOURCE=src\main.c
set NO_WARNINGS=-Wno-unused-function -Wno-missing-braces -Wno-unused-variable -Wno-unused-parameter
set WEB=0
set AUTOGEN=1

:: Procesar argumentos
for %%A in (%*) do (
    if "%%A"=="-n" (
        set AUTOGEN=0
    )
    if "%%A"=="-w" (
        set WARNINGS=-Wall -Wextra
    )
    if "%%A"=="-d" (
        set DEBUG=-DDEBUG
    )
)

:: Compilar autogen (equivalente al bloque comentado en bash)
gcc -w src\autogen.c -o build\autogen.exe

:: Crear carpeta build si no existe
if not exist build (
    mkdir build
)

:: Ejecutar autogen
cd build
autogen.exe
cd ..

:: Compilar el proyecto principal
gcc %SOURCE% %WARNINGS% %NO_WARNINGS% %DEBUG% -g3 -fshort-enums -o build\main.exe

endlocal
