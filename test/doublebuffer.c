#include "SDL.h"
#include <stdio.h>

static SDL_Surface *screen;
static Uint32 width = 800;
static Uint32 height = 600;

static Uint32 time = 5;

static void countdown(Uint32 t)
{
	for ( ; t  > 0; t--)
	{
		printf("%u...\n", t);
		SDL_Delay(1000);
	}
}

static void draw(Uint32 t)
{
	Uint32 start = SDL_GetTicks();

	Uint32 black = SDL_MapRGB(screen->format, 0, 0, 0);
	Uint32 white = SDL_MapRGB(screen->format, 255, 255, 255);

	Uint32 frames = 0;

	while((SDL_GetTicks() - start) <= t * 1000)
	{
		SDL_Rect r;
		r.x = 0;
		r.y = 0;
		r.w = width;
		r.h = height;

		SDL_FillRect(screen, &r, black);

		r.y = height / 2;
		r.h = height / 2;

		SDL_FillRect(screen, &r, white);

		SDL_Flip(screen);
		frames++;
	}

	printf("Frames per second: %f\n", frames / (float)t);
}

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) == 0)
	{
		Uint32 depth = 32;
		Uint32 flags = SDL_FULLSCREEN | SDL_HWSURFACE;

		puts("Testing single-buffer HW surface - expecting flickering");
		countdown(3);

		screen = SDL_SetVideoMode(width, height, depth, flags);
		draw(time);

		SDL_SetVideoMode(100, 100, 32, 0);

		puts("Testing double-buffer HW surface - expecting NO flickering");
		countdown(3);

		screen = SDL_SetVideoMode(width, height, depth, flags | SDL_DOUBLEBUF);
		draw(time);

		SDL_Quit();
	}

	return 0;
}
