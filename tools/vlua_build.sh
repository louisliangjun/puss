gcc -s -O2 -pthread -Wall -Iinclude -Ipuss_toolkit -o tools/vlua tools/vlua.c puss_toolkit/*.c -llua -lm -ldl

