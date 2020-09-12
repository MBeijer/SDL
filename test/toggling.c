#include "SDL.h"
#include <stdio.h>

static SDL_Surface *screen;
static Uint32 width = 800;
static Uint32 height = 600;
static Uint32 time = 5;

static void draw(Uint32 t, SDL_Surface *s)
{
	Uint32 start = SDL_GetTicks();
	Uint32 frames = 0;

	while((SDL_GetTicks() - start) <= t * 1000)
	{
		SDL_Rect r;
		r.x = rand() % width;
		r.y = rand() % height;

		SDL_BlitSurface(s, NULL, screen, &r);

		SDL_Flip(screen);
		frames++;
	}

	printf("Frames per second: %f\n", frames / (float)t);
}

static SDL_Surface* makeSurface()
{
	SDL_Surface *s = SDL_LoadBMP("sample.bmp");

	if (s)
	{
		SDL_Surface *new = SDL_DisplayFormatAlpha(s);

		if (new)
		{
			SDL_FreeSurface(s);
			s = new;
		}
		else
		{
			printf("Failed to convert\n");
		}

		if (SDL_SetAlpha(s, SDL_SRCALPHA, 100))
		{
			printf("SetAlpha failed: %s\n", SDL_GetError());
		}
	}

	return s;
}

int main(void)
{
	if (SDL_Init(SDL_INIT_VIDEO) == 0)
	{
		Uint32 depth = 32;
		Uint32 flags = SDL_HWSURFACE | SDL_DOUBLEBUF;

		screen = SDL_SetVideoMode(width, height, depth, flags | SDL_FULLSCREEN);

		SDL_Surface *s = makeSurface();

		draw(time, s);

#if 1
		screen = SDL_SetVideoMode(width, height, depth, flags);
#else
		if (SDL_WM_ToggleFullScreen(screen) != 1)
		{
			printf("Toggle failed: %s\n", SDL_GetError());
		}
#endif

		draw(time, s);

		SDL_FreeSurface(s);
		SDL_Quit();
	}

	return 0;
}
