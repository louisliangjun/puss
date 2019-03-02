@rem SET PATH=%%PATH%%;e:\msys2\mingw64\bin
@cd /D %~dp0\..
@set LUA=3rd/lua-5.3.5/src
@setlocal EnableDelayedExpansion
@set SRCS=tools/vlua.c
@for /F %%i IN ('dir /B /ON "%LUA%\*.c"') do @if "%%i" NEQ "lua.c" @if "%%i" NEQ "luac.c" @set SRCS=!SRCS! !LUA!/%%i
gcc -s -O2 -I%LUA% -o tools/vlua %SRCS% -lm
