#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
//TODO: remove this and load dynamically
#include <X11/extensions/Xinerama.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define uint64 uint64_t
#define uint8 uint8_t
#define uint32 uint32_t

//Xinerama type declarations
//typedef struct {
//		int   screen_number;
//	 	short x_org;
//	 	short y_org;
//	 	short width;
//	 	short height;
//} XineramaScreenInfo;
//
//typedef int (*XineramaQueryExtension_t)(Display*, int*, int*);
//typedef int (*XineramaIsActive_t)(Display*);
//typedef int (*XineramaIsActive_t)(Display*);
//end

#define ScreenInfo XineramaScreenInfo

int getShiftAmount(unsigned long mask) {
	if (mask == 0) return 0;

	int shift = 0;     // Count how many trailing zeros are in the binary mask
	while ((mask & 1) == 0) {
   		mask >>= 1;
		shift++;
	}
	return shift;
}

ScreenInfo getActiveScreenFromXinerama(Display *display, Window *root_window_out) {
	int screenCount = 0;
	ScreenInfo *screensInfo = XineramaQueryScreens(display, &screenCount);
	printf("xinerama num of screens %d\n", screenCount);
	*root_window_out = RootWindow(display, 0);

	Window root_return, child_return;
	int root_x, root_y, win_x, win_y;
	uint32 mask_return;
	if (XQueryPointer(display, *root_window_out, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask_return)) {
		for (int i = 0; i < screenCount; i++) {
			ScreenInfo currScreen = screensInfo[i];
			if (
				(root_x >= currScreen.x_org && root_x < currScreen.x_org + currScreen.width) &&
				(root_y >= currScreen.y_org && root_y < currScreen.y_org + currScreen.height)
			) {
				return currScreen;
			}

		}
	}

	ScreenInfo failed = {0};
	failed.screen_number = -1;
	return failed;
}

ScreenInfo getActiveScreen(Display *display, Window *root_window_out) {
	int screen = -1;
	int screenCount = ScreenCount(display);

	for (int i = 0; i < screenCount; ++i) {
		*root_window_out = RootWindow(display, i);

		Window trash, trash1;
		int trash2, trash3, trash4, trash5;
		uint32 trash6;

		if (XQueryPointer(display, *root_window_out, &trash, &trash1, &trash2, &trash3, &trash4, &trash5, &trash6)) {
			screen = i;
			break;
		}
	}

	int width = DisplayWidth(display, screen);
	int height = DisplayHeight(display, screen);

	return (ScreenInfo) {.screen_number=screen, .x_org=0, .y_org=0, .width=width, .height=height};
}

int main() {
	Display *display = XOpenDisplay(NULL);

	if (display == NULL) {
		printf("Could not open display\n");
		return 1;
	}

	Window root_window;

//TODO: I need to use the xinerama extension to capture only one screen if it's active because it merges all into a single
// x11 screen
// I will load this dynamically so it still works in set ups without it
    // temporal test
    //void *xinerama_lib = dlopen("libXinerama.so.1", RTLD_LAZY);
    //if (xinerama_lib == NULL) {
    //    printf("Failed to load xinerama lib");
    //    return 1;
    //}
    //XineramaQueryExtension_t XineramaQueryExtension = (XineramaQueryExtension_t) dlsym(xinerama_lib, "XineramaQueryExtension");
    //XineramaIsActive_t XineramaIsActive = (XineramaIsActive_t) dlsym(xinerama_lib, "XineramaIsActive");
    int event_base, error_base;
	ScreenInfo screen;
    if (XineramaQueryExtension(display, &event_base, &error_base) && XineramaIsActive(display)) {
        printf("Xinerama is active\n");
		screen = getActiveScreenFromXinerama(display, &root_window);
    } else {
		screen = getActiveScreen(display, &root_window);
	}

	if (screen.screen_number == -1) {
		printf("Couldn't get screen\n");
		return 1;
	}

    // temporal test

	printf("Dimensions %dx%d\n", screen.width, screen.height);

	XImage *image = XGetImage(display, root_window, screen.x_org, screen.y_org, screen.width, screen.height, AllPlanes, ZPixmap);
	if (image == NULL) {
		printf("Could not get image\n");
		return 1;
	}

	size_t memory_size = image->width * image->height * (image->bits_per_pixel * 4);
	uint32 *png_data = malloc(memory_size);

	for (int row = 0; row < image->height; ++row) {
		for (int col = 0; col < image->width; ++col) {
			int index = (row * image->width) + col;
			uint32 pixel = (((uint32 *) image->data)[index]);

			uint32 r_shift = getShiftAmount(image->red_mask);
			uint32 g_shift = getShiftAmount(image->green_mask);
			uint32 b_shift = getShiftAmount(image->blue_mask);

			uint8 r = (pixel & image->red_mask) >> r_shift;
			uint8 g = (pixel & image->green_mask) >> g_shift;
			uint8 b = (pixel & image->blue_mask) >> b_shift;

			pixel = 0xff000000; // reset to 0 except for alpha

			// switch r and b
			pixel = pixel | (r << b_shift);
			pixel = pixel | (g << g_shift);
			pixel = pixel | (b << r_shift);

			png_data[index] = pixel;
		}
	}

    stbi_write_png("screenshot.png", image->width, image->height, 4, png_data, image->width * 4);
	free(png_data);
}
