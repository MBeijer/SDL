/*

gcc alphablit.c -Isdk:local/common/include/SDL -use-dynld -lSDL -lauto -g -Wall -O2

*/

#include "SDL/SDL.h"

#include <stdio.h>
#include <proto/exec.h>

#define BENCHMARK_VERSION "0.2"

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

#define WIDTH 640
#define HEIGHT 480

#define BLIT_WIDTH 100
#define BLIT_HEIGHT 100
#define BLITS_PER_ITERATION 100

#define ALPHA 0x80000000

static SDL_bool quit = SDL_FALSE;

static SDL_Surface * view;

static SDL_Surface * red;
static SDL_Surface * green;
static SDL_Surface * blue;

static const SDL_Color RED_COLOR = { 255,   0,   0, 255 };
static const SDL_Color GREEN_COLOR = {   0, 255,   0, 255 };
static const SDL_Color BLUE_COLOR = {   0,   0, 255, 255 };

#if 0
#define DEBUGME IExec->DebugPrintF("%s %d\n", __FUNCTION__, __LINE__);
#else
#define DEBUGME
#endif

static void checkVideoInfo()
{
	const SDL_VideoInfo * vi = SDL_GetVideoInfo();

	printf("Video Info: HW %d, HW blit %d, HW blit A %d, HW blit CC %d, SW blit %d\n",
		vi->hw_available, vi->blit_hw, vi->blit_hw_A, vi->blit_hw_CC, vi->blit_sw);
}

/*
static void debugSurface(SDL_Surface *surface)
{
	printf("Surface %d*%d (bits %d, bytes %d) flags: 0x%X\n",
		surface->w, surface->h,
		surface->format->BitsPerPixel,
		surface->format->BytesPerPixel,
		surface->flags);
}
*/

static void paintTexture32(SDL_Surface *texture, Uint32 color, SDL_bool useColorKey, Uint32 colorKey)
{
	Uint32 * p = (Uint32 *)texture->pixels;

	const int pitch = texture->pitch / 4;
	color |= ALPHA;

	//IExec->DebugPrintF("Pixels %x, pitch %d\n", p, pitch);

	int y;
	for (y = 0; y < texture->h; y++, p += pitch )
	{
		int x;

		for (x = 0; x < texture->w; x++ )
		{
			p[x] = color;
		}

		if (useColorKey)
		{
			// Make stripes
			for (x = 1; x < texture->w - 1; x++ )
			{
				if ((x % 2) == 1)
				{
					p[x] = colorKey;
				}
			}
		}
	}
}

static void paintTexture16(SDL_Surface* texture, Uint16 color, SDL_bool useColorKey, Uint16 colorKey)
{
	Uint16 * p = (Uint16 *)texture->pixels;

	const int pitch = texture->pitch / 2;

	int y;
	for (y = 0; y < texture->h; y++, p += pitch )
	{
		int x;

		for (x = 0; x < texture->w; x++ )
		{
			p[x] = color;
		}

		if (useColorKey)
		{
			// Make stripes
			for (x = 1; x < texture->w - 1; x++ )
			{
				if ((x % 2) == 1)
				{
					p[x] = colorKey;
				}
			}
		}
	}
}

static void paintTexture8(SDL_Surface* texture, Uint8 color, SDL_bool useColorKey, Uint16 colorKey)
{
	Uint8* p = (Uint8 *)texture->pixels;
	//printf("Color %d\n", color);
	const int pitch = texture->pitch;

	int y;

	for(y = 0; y < texture->h; y++, p += pitch)
	{
		int x;

		for (x = 0; x < texture->w; x++)
		{
			p[x] = color;
		}

		if (useColorKey)
		{
			// Make stripes
			for (x = 1; x < texture->w - 1; x++ )
			{
				if ((x % 2) == 1)
				{
					p[x] = colorKey;
				}
			}
		}
	}
}

static SDL_Surface *allocTexture(Uint32 depth, int useHardware, int usePerSurfaceAlpha)
{
	int flags = (useHardware) ? SDL_HWSURFACE : SDL_SWSURFACE;

	Uint32 rmask = 0x0;
	Uint32 gmask = 0x0;
	Uint32 bmask = 0x0;
	Uint32 amask = 0x0;

	if (depth == 32)
	{
		flags |= SDL_SRCALPHA;

		rmask = 0x00FF0000;
		gmask = 0x0000FF00;
		bmask = 0x000000FF;
		amask = (usePerSurfaceAlpha) ? 0x00 : 0xFF000000; // TODO: there is always Amask with 32-bit SDL_HWSURFACE
	}
	else if (depth == 16)
	{
		//
	}

	return SDL_CreateRGBSurface(flags, BLIT_WIDTH, BLIT_HEIGHT, depth,
		rmask, gmask, bmask, amask);
}

// Create a simple palette for 8-bit displays
static void createPalette(SDL_Surface * surface)
{
	// r, g, b, unused
	SDL_Color colors[] = {
		{  0,   0,   0, 255}, // black
		{255, 255, 255, 255}, // white
		{255,   0,   0, 255}, // red
		{  0, 255,   0, 255}, // green
		{  0,   0, 255, 255}, // blue
		};

	SDL_SetColors(surface, colors, 0, sizeof(colors) / sizeof(SDL_Color));
}

typedef struct ModeInfo
{
	int useFullscreen;
	int useHardware;
	int useSurfaceAlpha;
	int useColorKey;
	int depth;
	int iterations;
}  ModeInfo;

static SDL_Surface * createTexture(SDL_Color color, ModeInfo mi)
{
	SDL_Surface * texture = allocTexture(mi.depth, mi.useHardware, mi.useSurfaceAlpha);

	if (texture == NULL)
	{
		puts("Couldn't create surface");

		return NULL;
	}

	if (mi.depth == 8)
	{
		createPalette(texture);
	}
	else if (mi.useSurfaceAlpha)
	{
		SDL_SetAlpha(texture, SDL_SRCALPHA, 0x80);
		//printf("psa %d am 0x%x\n", texture->format->alpha, texture->format->Amask);
	}

	//debugSurface(texture);

	Uint32 colorKey = 0;

	if (mi.useColorKey)
	{
		colorKey = SDL_MapRGB(texture->format, 0xFF, 0xFF, 0xFF);
		//printf("Color key: 0x%X\n", colorKey);
	}

	Uint32 surfaceColor = SDL_MapRGB(texture->format, color.r, color.g, color.b);

	if (SDL_MUSTLOCK(texture))
	{
		if (SDL_LockSurface(texture) < 0)
		{
			return texture;
		}
	}

	if (mi.depth == 32)
	{
	    paintTexture32(texture, surfaceColor, mi.useColorKey, colorKey);
	}
	else if (mi.depth == 16)
	{
		paintTexture16(texture, surfaceColor, mi.useColorKey, colorKey);
	}
	else if (mi.depth == 8)
	{
		paintTexture8(texture, surfaceColor, mi.useColorKey, colorKey);
	}

	if (SDL_MUSTLOCK(texture))
	{
		SDL_UnlockSurface(texture);
	}

	if (mi.useColorKey)
	{
		SDL_SetColorKey(texture, SDL_SRCCOLORKEY, colorKey);
	}

	return texture;
}

static void freeTextures()
{
	if (red)
	{
		SDL_FreeSurface(red);
		red = NULL;
	}

	if (green)
	{
		SDL_FreeSurface(green);
		green = NULL;
	}

	if (blue)
	{
		SDL_FreeSurface(blue);
		blue = NULL;
	}
}

static void generateTextures(ModeInfo mi)
{
	freeTextures();

	red = createTexture(RED_COLOR, mi);
	green = createTexture(GREEN_COLOR, mi);
	blue = createTexture(BLUE_COLOR, mi);
}


static const char * getAlphaString(Uint32 type)
{
	static const char * strings[] = { "Per-pixel", "Per-surface", "No" };

	if (type < sizeof(strings) / sizeof(strings[0]))
	{
		return strings[type];
	}

	return strings[2];
}

static void setMode(ModeInfo mi)
{
	int flags = (mi.useFullscreen ? SDL_FULLSCREEN : 0) | (mi.useHardware ? SDL_HWSURFACE : SDL_SWSURFACE) | SDL_HWPALETTE;

	printf("Requested mode: %d-bit %s %s. %s alpha, %scolor key\n",
		mi.depth,
		mi.useHardware ? "accelerated" : "non-accelerated",
		mi.useFullscreen ? "fullscreen" : "window",
		getAlphaString(mi.useSurfaceAlpha),
		mi.useColorKey ? "" : "no ");

	view = SDL_SetVideoMode(WIDTH, HEIGHT, mi.depth, flags);

	if (!view)
	{
		printf("SDL_SetVideoMode: %s\n", SDL_GetError());
		return;
	}

	//printf("Display surface flags: 0x%X\n", view->flags);

	generateTextures(mi);

	if (mi.depth == 8)
	{
		createPalette(view);
	}

	//SDL_Delay(1000);
}

static void draw(SDL_Surface * texture, Uint32 sleep)
{
	int i;

	for (i = 0; i < BLITS_PER_ITERATION; i++)
	{
		SDL_Rect s, d;

		s.w = BLIT_WIDTH;
		s.h = BLIT_HEIGHT;
		s.x = 0;
		s.y = 0;

		d.w = BLIT_WIDTH;
		d.h = BLIT_HEIGHT;
		d.x = rand() % (WIDTH - BLIT_WIDTH);
		d.y = rand() % (HEIGHT - BLIT_HEIGHT);

		if (SDL_BlitSurface(texture, &s, view, &d))
		{
			printf("SDL_BlitSurface: %s\n", SDL_GetError());
			return;
		}
	}

	if (SDL_Flip(view))
	{
		printf("SDL_Flip: %s\n", SDL_GetError());
	}

	if (sleep)
	{
		SDL_Delay(sleep);
	}
}

static SDL_bool checkQuit()
{
	SDL_Event e;

	while(SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_KEYDOWN:
			{
				if (e.key.keysym.sym == SDLK_q)
				{
					puts("*** QUIT ***");
					quit = SDL_TRUE;
				}
			} break;
		}
	}

	return quit;
}

static void test(Uint32 bytesPerPixel, Uint32 iterations, Uint32 sleep)
{
	Uint32 start = SDL_GetTicks();

	Uint32 i;

	for (i = 0; i < iterations; i++)
	{
		if (checkQuit())
		{
			return;
		}

		draw(red, sleep);
		draw(green, sleep);
		draw(blue, sleep);
	}

	Uint32 finish = SDL_GetTicks();

	Uint32 duration = finish - start;

	Uint32 frames = iterations * 3;
	Uint32 blits = frames * BLITS_PER_ITERATION;
	Uint64 bytes = blits * BLIT_WIDTH * BLIT_HEIGHT * bytesPerPixel;

	if (duration == 0)
	{
		puts("Duration 0");
		return;
	}

	printf("RESULT: duration %u ms, %.1f frames/s, %u blits, %.1f blits/s, %llu bytes, %llu bytes/s\n",
		duration,
		1000.0f * frames / duration,
		blits,
		1000.0f * blits / duration,
		bytes,
		1000 * bytes / duration);
}


static void parseArgs(int argc, char* argv[], Uint32 *iterations, Uint32 *sleep)
{
	if (argc > 1)
	{
		*iterations = atoi(argv[1]);

		if (argc > 2)
		{
			*sleep = atoi(argv[2]);
		}
	}

	printf("Iterations=%d, Blits=%d, Delay=%d ms\n", *iterations, BLITS_PER_ITERATION, *sleep);
}

static void checkVersion(void)
{
	SDL_version compiled;

	SDL_VERSION(&compiled);

	puts("SDL blit benchmark, version " BENCHMARK_VERSION);

	printf("Compiled library version: %d.%d.%d\n",
			compiled.major, compiled.minor, compiled.patch);

	printf("Linked library version: %d.%d.%d\n",
			SDL_Linked_Version()->major,
			SDL_Linked_Version()->minor,
			SDL_Linked_Version()->patch);
}

static void printHelp(void)
{
	puts("USAGE: 'alphablit <ITER> <DELAY>', where");
	puts("\t<ITER> is number of test iterations per mode and");
	puts("\t<DELAY> is delay in milliseconds after each blit sequence, for visual debugging.");
}

static void runTest(ModeInfo mi, Uint32 iterations, Uint32 sleep)
{
	setMode(mi);

	if (view)
	{
		test(mi.depth / 8, MIN(mi.iterations, iterations), sleep);
	}
	else
	{
		puts("Failed to open screen");
	}
}

#define WINDOW 0
#define FULLSCREEN 1

#define SW 0
#define HW 1

#define PER_PIXEL_ALPHA 0
#define PER_SURFACE_ALPHA 1
#define NO_ALPHA 2

#define NO_COLOR_KEY 0
#define USE_COLOR_KEY 1

static void runTests(Uint32 iterations, Uint32 sleep)
{
	ModeInfo tests[] =
	{
#if 1
		{ WINDOW, SW, PER_PIXEL_ALPHA, NO_COLOR_KEY, 16, 100 },
		{ WINDOW, SW, PER_PIXEL_ALPHA, NO_COLOR_KEY, 32, 100 },
		{ WINDOW, SW, PER_SURFACE_ALPHA, NO_COLOR_KEY, 16, 100 },
		{ WINDOW, SW, PER_SURFACE_ALPHA, NO_COLOR_KEY, 32, 100 },

		{ WINDOW, SW, PER_SURFACE_ALPHA, USE_COLOR_KEY, 16, 100 },
		{ WINDOW, SW, PER_SURFACE_ALPHA, USE_COLOR_KEY, 32, 100 },

		{ FULLSCREEN, SW, NO_ALPHA, NO_COLOR_KEY, 8, 100 },
		{ FULLSCREEN, SW, PER_PIXEL_ALPHA, NO_COLOR_KEY, 16, 100 },
		{ FULLSCREEN, SW, PER_PIXEL_ALPHA, NO_COLOR_KEY, 32, 100 },

		{ FULLSCREEN, SW, PER_SURFACE_ALPHA, NO_COLOR_KEY, 16, 100 },
		{ FULLSCREEN, SW, PER_SURFACE_ALPHA, NO_COLOR_KEY, 32, 100 },

		{ FULLSCREEN, SW, NO_ALPHA, USE_COLOR_KEY, 8, 5 },
		{ FULLSCREEN, SW, PER_SURFACE_ALPHA, USE_COLOR_KEY, 16, 100 },
		{ FULLSCREEN, SW, PER_SURFACE_ALPHA, USE_COLOR_KEY, 32, 100 },
#endif
	};

	int t;
	for (t = 0; t < (sizeof(tests) / sizeof(ModeInfo)); t++)
	{
		if (quit == SDL_FALSE)
		{
			IExec->DebugPrintF("...Running test #%d\n", t);
			printf("...Running test #%d...\n", t);

			tests[t].useHardware = SW;
			runTest(tests[t], iterations, sleep);

			tests[t].useHardware = HW;
			runTest(tests[t], iterations, sleep);
		}
	}
}

int main(int argc, char* argv[])
{
	Uint32 iterations = 50;
	Uint32 sleep = 0;

	if (SDL_Init(SDL_INIT_VIDEO))
	{
		printf("SDL_Init: %s\n", SDL_GetError());
		return 0;
	}

	checkVersion();
	printHelp();

	checkVideoInfo();

	parseArgs(argc, argv, &iterations, &sleep);

	printf("Display size %d*%d, blit size %d*%d\n", WIDTH, HEIGHT, BLIT_WIDTH, BLIT_HEIGHT);

	runTests(iterations, sleep);

	freeTextures();

	SDL_Quit();

	return 0;
}
