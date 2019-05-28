# vlua build: gcc -s -O2 -pthread -Wall -o tools/vlua tools/vlua.c -llua -lm -ldl

SRCS=`ls 3rd/lua-5.3.5/src/*.c | grep -v lua.c | grep -v luac.c`
#echo SRCS: $SRCS
gcc -s -O2 -pthread -Wall -DLUA_USE_LINUX -I3rd/lua-5.3.5/src -o tools/vlua ./tools/vlua.c $SRCS -lm -ldl
