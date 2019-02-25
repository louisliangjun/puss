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
  |     +-- <plugins>         # plugin plugins
  |
  +-- <include>    # puss core headers
  |     |
  |     +-- puss_macros.h
  |     +-- puss_plugin.h
  |     +-- puss_lua.h
  |
  +-- <plugins>    # puss plugins source
  |     |
  |     +-- <...plugin...>    # plugin plugins
  |
  +-- <puss_toolkit> # puss toolkit lib
  |
  +-- <puss>       # puss exe project source
  |
  +-- <tools>
  |     |
  |     +-- vlua.exe          # prebuild vlua in win32
  |     +-- vlua.c            # vlua source code
  |     +-- vlua_build.cmd    # build vlua script in mingw
  |     +-- vlua_build.sh     # build vlua script in linux
  |     +-- vmake.lua         # vmake base script
  |
  +-- vmake        # puss solution makefile
  +-- vmake.cmd    # for windows
  +-- README.md	   # this file
```

vlua - make tool with lua script
-----------

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

