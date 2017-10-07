puss
===========

* toolset for C/lua develop
 
project file struct
-----------

```
<this-dir>
  |
  +-- <3rd>        # some useful thrid-part-libs
  |
  +-- <bin>
  |     |
  |     +-- <modules>         # moudles
  |           |
  |           +-- <some_module>    # some module for dev & plugin
  |
  +-- <tools>
  |     |
  |     +-- vlua.exe          # NOTICE : need build from vlua.c & lua53
  |     +-- vlua.c            # vlua source code
  |     +-- vmake_base.lua    # vmake used lua utils
  |
  +-- README.md	   # this file
```

vlua - make tool with lua script
-----------

* linux compile: gcc -s -O2 -pthread -Wall -o vlua ./vlua.c -llua -lm -ldl
* mingw compile: gcc -s -O2 -Wall -I./3rd/lua53 -o vlua ./vlua.c ./3rd/lua53/*.c -lm

* more tips: see ./tools/vlua.c

* all projects use one make script: ./vmake
* usage : ./vmake [targets] [-options], options MUST startswith "-"
* examples :
```
./vmake              # compile all release
./vmake -debug -j8   # compile all debug, use 8 thread compile, default use 4 threads
./vmake clean        # clean all
./vmake -debug lua53 # compile lua53 debug
./vmake lua53 puss   # compile lua53 and puss
```

