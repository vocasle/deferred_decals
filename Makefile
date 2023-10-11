CGLAGS = -g

SRCS := $(shell find src -name '*.c')
OBJS := $(shell find src -name '*.c')

RES_PATH=\"$(shell pwd)/res\"

all: glad decals

glad:
	cc -c thirdparty/glad/src/gl.c -Ithirdparty/glad/include -o build/glad.o
	ar rcs build/libglad.a build/glad.o

decals: $(SRCS) build/libglad.a
	echo SOURCES: $(SRCS)
	cc -DRES_HOME=$(RES_PATH) $(SRCS) -g -o build/$@ -lm -lglfw -Lbuild -lglad -Ithirdparty/glad/include 

clean:
	rm -rm build 
