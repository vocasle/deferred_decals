CGLAGS = -g

SRCS := $(shell find src -name '*.c')
OBJS := $(shell find src -name '*.c')

RES_PATH=\"$(shell pwd)/res\"

INCLUDE_DIRS := -Ithirdparty -Ithirdparty/glad/include

all: glad decals

glad:
	cc -c thirdparty/glad/src/gl.c -Ithirdparty/glad/include -o build/glad.o
	ar rcs build/libglad.a build/glad.o

decals: $(SRCS) build/libglad.a
	cc -DRES_HOME=$(RES_PATH) $(SRCS) -g -o build/$@ -lm -lglfw -Lbuild -lglad $(INCLUDE_DIRS) 

clean:
	rm -rm build 
