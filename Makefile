all:
	g++ main.cpp -o main.exe -I sdl/include -L sdl/lib -lSDL3 -lSDL3_image

