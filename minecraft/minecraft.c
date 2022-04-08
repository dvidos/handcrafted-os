#include <stdio.h>
#include <stdbool.h>
#include "SDL.h"

#define SCREEN_WIDTH   1280
#define SCREEN_HEIGHT  720

SDL_Renderer *renderer;
SDL_Window *window;

void init() {
	int rendererFlags, windowFlags;

	rendererFlags = SDL_RENDERER_ACCELERATED;
	windowFlags = 0;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	window = SDL_CreateWindow("Shooter 01", 
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
			SCREEN_WIDTH, SCREEN_HEIGHT, windowFlags);
	if (!window) {
		printf("Failed to open %d x %d window: %s\n", SCREEN_WIDTH, SCREEN_HEIGHT, SDL_GetError());
		exit(1);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	renderer = SDL_CreateRenderer(window, -1, rendererFlags);
	if (!renderer) {
		printf("Failed to create renderer: %s\n", SDL_GetError());
		exit(1);
	}
}

void prepare_scene(void)
{
	SDL_SetRenderDrawColor(renderer, 96, 128, 255, 255);
	SDL_RenderClear(renderer);
}

void present_scene(void)
{
	SDL_RenderPresent(renderer);
}

void handle_input(void)
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
				exit(0);
				break;
			default:
				break;
		}
	}
}

void cleanup() {
}


void main(int argc, char *argv[]) {
	init();
	atexit(cleanup);

	while (true) {
		prepare_scene();
		handle_input();
		present_scene();
		SDL_Delay(16);
	}
}

