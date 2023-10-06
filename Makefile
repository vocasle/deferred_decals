all: glad decals

glad:
	cc -c thirdparty/glad/src/gl.c -Ithirdparty/glad/include -o build/glad.o
	ar rcs build/libglad.a build/glad.o

decals: src/decals.c build/libglad.a
	cc src/decals.c -g -o build/decals -lglfw -Lbuild -lglad -Ithirdparty/glad/include

clean:
	rm -rm build 
