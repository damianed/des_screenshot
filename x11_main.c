#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
//TODO: remove this and load xrandr dynamically
#include <X11/extensions/Xrandr.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define uint64 uint64_t
#define uint8 uint8_t
#define uint32 uint32_t

typedef struct {
 int valid;
 int x, y;
 uint32 width, height;
} ScreenInfo;

int getShiftAmount(unsigned long mask) {
	if (mask == 0) return 0;

	int shift = 0;     // Count how many trailing zeros are in the binary mask
	while ((mask & 1) == 0) {
   		mask >>= 1;
		shift++;
	}
	return shift;
}

ScreenInfo getActiveScreenFromXrandr(Display *display, Window *root_window_out) {
	ScreenInfo result = {.valid = 0};
	*root_window_out = RootWindow(display, 0);
	XRRScreenResources *screens = XRRGetScreenResources(display, *root_window_out);
	if (!screens) {
		printf("Failed to get resources from xrandr\n");
		return result;
	}

	Window root_return, child_return;
	int root_x, root_y, win_x, win_y;
	uint32 mask_return;
	if (XQueryPointer(display, *root_window_out, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask_return)) {

		if (root_x >= 0 && root_y >= 0) {
			for (int i = 0; i < screens->ncrtc; i++) {
				XRRCrtcInfo *info = XRRGetCrtcInfo(display, screens, screens->crtcs[i]);
				
				if (
					(root_x >= info->x && (uint32) root_x < info->x + (uint32) info->width) &&
					(root_y >= info->y && (uint32) root_y < info->y + (uint32) info->height)
				) {
					result.valid = 1;
					result.x = info->x;
					result.y = info->x;
					result.width = info->width;
					result.height = info->height;
					break;
				}
			}
		} else {
			printf("Invalid pointer coordinates returned\n");
		}

	} else {
		printf("Failed to query pointer\n");
	}
		

	XRRFreeScreenResources(screens);
	return result;
}

ScreenInfo getActiveScreen(Display *display, Window *root_window_out) {
	int screen = -1;
	int screenCount = ScreenCount(display);

	for (int i = 0; i < screenCount; i++) {
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
	ScreenInfo result = {0, 0, 0, width, height};
	if (screen >= 0) {
		result.valid = 1;
	}

	return result;
}

int main() {
	Display *display = XOpenDisplay(NULL);

	if (display == NULL) {
		printf("Could not open display\n");
		return 1;
	}

	Window root_window;

    int event_base, error_base;
	ScreenInfo screen;
	if (XRRQueryExtension(display, &event_base, &error_base)) {
		//TODO: this seems very slow, measure and figure out a way to make it faster
		screen = getActiveScreenFromXrandr(display, &root_window);
    } else {
		printf("xrandr is not active\n");
		screen = getActiveScreen(display, &root_window);
	}

	if (!screen.valid) {
		printf("Couldn't get screen\n");
		return 1;
	}

	printf("Active screen dimensions %dx%d\n", screen.width, screen.height);

	XImage *image = XGetImage(display, root_window, screen.x, screen.y, screen.width, screen.height, AllPlanes, ZPixmap);
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
