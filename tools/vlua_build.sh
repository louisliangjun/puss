# vlua build: gcc -s -O2 -pthread -Wall -Iinclude -Ipuss_toolkit -o tools/vlua tools/vlua.c puss_toolkit/*.c -llua -lm -ldl

SRCS=`ls 3rd/lua-5.3.5/src/*.c | grep -v lua.c | grep -v luac.c`
#echo SRCS: $SRCS
gcc -s -O2 -pthread -Wall -DLUA_USE_LINUX -Iinclude -Ipuss_toolkit -o tools/vlua tools/vlua.c puss_toolkit/*.c $SRCS -lm -ldl

