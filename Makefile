CGLAGS = -g

SRCS := $(shell find src -name '*.c')
OBJS := $(shell find src -name '*.c')

RES_PATH=\"$(shell pwd)/res\"

INCLUDE_DIRS := -Ithirdparty -Ithirdparty/glad/include -Ithirdparty/mikktspace
MACRO_DEFINES := -DRES_HOME=$(RES_PATH) 

all: glad decals mikktspace

glad:
	cc -c thirdparty/glad/src/gl.c -Ithirdparty/glad/include -o build/glad.o
	ar rcs build/libglad.a build/glad.o

mikktspace:
	cc -c thirdparty/mikktspace/mikktspace.c -Ithirdparty/mikktspace -o build/mikktspace.o
	ar rcs build/libmikktspace.a build/mikktspace.o

decals: $(SRCS) build/libglad.a mikktspace 
	cc $(MACRO_DEFINES) $(SRCS) -g -o build/$@ -lm -lglfw -Lbuild -lglad -lmikktspace $(INCLUDE_DIRS) 

clean:
	rm -rm build 
