#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define uint64 uint64_t
#define uint8 uint8_t
#define uint32 uint32_t
#define PPM_HEADER_LEN 18 

int getShiftAmount(unsigned long mask) {
	if (mask == 0) return 0;

	int shift = 0;     // Count how many trailing zeros are in the binary mask
	while ((mask & 1) == 0) {
   		mask >>= 1;
		shift++;
	}
	return shift;
}

int getActiveScreen(Display *display, Window *root_window_out) {
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

	return screen;
}

int main() {
	Display *display = XOpenDisplay(NULL);

	if (display == NULL) {
		printf("Could not open display\n");
		return 1;
	}

	Window root_window;
	int screen = getActiveScreen(display, &root_window);

	if (screen == -1) {
		printf("Failed to get active screen\n");
		return 1;
	}

	int width = DisplayWidth(display, screen);
	int height = DisplayHeight(display, screen);
	
	XImage *image = XGetImage(display, root_window, 0, 0, width, height, AllPlanes, ZPixmap);

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
