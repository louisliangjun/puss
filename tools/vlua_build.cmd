rem SET PATH=%%PATH%%;e:\msys2\mingw64\bin
@echo mingw build vlua, PATH: %PATH%
@cd /D %~dp0\..
@set LUA=3rd/lua-5.3.5/src
gcc -s -O2 -I%LUA% -Iinclude -Ipuss_toolkit -o tools/vlua^
 tools/vlua.c puss_toolkit/*.c^
 %LUA%/lapi.c %LUA%/lauxlib.c %LUA%/lbaselib.c %LUA%/lbitlib.c^
 %LUA%/lcode.c %LUA%/lcorolib.c %LUA%/lctype.c %LUA%/ldblib.c^
 %LUA%/ldebug.c %LUA%/ldo.c %LUA%/ldump.c %LUA%/lfunc.c^
 %LUA%/lgc.c %LUA%/linit.c %LUA%/liolib.c %LUA%/llex.c^
 %LUA%/lmathlib.c %LUA%/lmem.c %LUA%/loadlib.c %LUA%/lobject.c^
 %LUA%/lopcodes.c %LUA%/loslib.c %LUA%/lparser.c %LUA%/lstate.c^
 %LUA%/lstring.c %LUA%/lstrlib.c %LUA%/ltable.c %LUA%/ltablib.c^
 %LUA%/ltm.c %LUA%/lundump.c %LUA%/lutf8lib.c %LUA%/lvm.c^
 %LUA%/lzio.c^
 -lm
