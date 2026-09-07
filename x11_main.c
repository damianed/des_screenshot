#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <dlfcn.h>
#include <time.h>
//TODO: remove this and load xrandr dynamically
#include <X11/extensions/Xrandr.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#define DES_TIME_DEBUG_IMPLEMENTATION
#include "lib/des_time_debug.h"

#define uint64 uint64_t
#define uint8 uint8_t
#define uint uint32_t

typedef struct {
 bool valid;
 int x, y;
 uint width, height;
} ScreenSection;

bool strEquals(char *s1, char *s2) {
#define MAX_LEN 255
    des_uint count = 0;
    while (count++ < MAX_LEN) {
        if ((*s1 == '\0' || *s2 == '\0') || (*s1++ != *s2++)) {
            break;
        }
    }

    if (*s1 == '\0' && *s2 == '\0') {
        return 1;
    }

    return 0;
}

int getShiftAmount(unsigned long mask) {
    if (mask == 0) {
        return 0;
    }

    int shift = 0;     // Count how many trailing zeros are in the binary mask
    while ((mask & 1) == 0) {
           mask >>= 1;
        shift++;
    }

    return shift;
}

ScreenSection getActiveScreenFromXrandr(Display *display, Window *root_window) {
    ScreenSection result = {.valid = 0};

    des_start_debug("Get Screens");
    XRRScreenResources *screens = XRRGetScreenResourcesCurrent(display, *root_window);
    des_end_debug("Get Screens");

    if (!screens) {
        printf("Failed to get resources from xrandr\n");
        return result;
    }

    des_start_debug("Query Pointer");
    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    uint mask_return;
    if (XQueryPointer(display, *root_window, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask_return)) {
        des_end_debug("Query Pointer");

        des_start_debug("Get Info");
        if (root_x >= 0 && root_y >= 0) {
            for (int i = 0; i < screens->ncrtc; i++) {
                XRRCrtcInfo *info = XRRGetCrtcInfo(display, screens, screens->crtcs[i]);

                if (
                    (root_x >= info->x && (uint) root_x < info->x + (uint) info->width) &&
                    (root_y >= info->y && (uint) root_y < info->y + (uint) info->height)
                ) {
                    result.valid = 1;
                    result.x = info->x;
                    result.y = info->y;
                    result.width = info->width;
                    result.height = info->height;
                    break;
                }
            }
        } else {
            printf("Invalid pointer coordinates returned\n");
        }

        des_end_debug("Get Info");
    } else {
        printf("Failed to query pointer\n");
    }

    des_start_debug("Free Screen");
    XRRFreeScreenResources(screens);
    des_end_debug("Free Screen");

    return result;
}

ScreenSection getActiveScreen(Display *display, Window *root_window_out) {
    int screen = -1;
    int screenCount = ScreenCount(display);

    for (int i = 0; i < screenCount; i++) {
        *root_window_out = RootWindow(display, i);

        Window trash, trash1;
        int trash2, trash3, trash4, trash5;
        uint trash6;

        if (XQueryPointer(display, *root_window_out, &trash, &trash1, &trash2, &trash3, &trash4, &trash5, &trash6)) {
            screen = i;
            break;
        }
    }

    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    ScreenSection result = {.valid=0, .x=0, .y=0, width, height};
    if (screen >= 0) {
        result.valid = 1;
    }

    return result;
}

ScreenSection getActiveWindow(Display *display, Window *root_window_out) {
    ScreenSection result = {0};
    Window focus_return;
    int revert_to_return;
    XGetInputFocus(display, &focus_return, &revert_to_return);

    XWindowAttributes attributes;
    XGetWindowAttributes(display, focus_return, &attributes);

    result.x = attributes.x;
    result.y = attributes.x;
    result.width = attributes.width;
    result.height = attributes.height;
    result.valid = 1;

    *root_window_out = attributes.root;

    return result;
}


int main(int argc, char *argv[]) {
    bool only_active_window = argc > 1 && strEquals(argv[1], "--active-window");

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        printf("Could not open display\n");
        return 1;
    }

    Window root_window;
    root_window = DefaultRootWindow(display);

    int event_base, error_base;
    ScreenSection section;

    des_start_debug("Query Extension");
    if (only_active_window) {
        des_start_debug("Get Active Window");
        section = getActiveWindow(display, &root_window);
        des_end_debug("Get Active Window");
    } else if (XRRQueryExtension(display, &event_base, &error_base)) {
        des_end_debug("Query Extension");

        des_start_debug("Get Active Screen");
        section = getActiveScreenFromXrandr(display, &root_window);
        des_end_debug("Get Active Screen");
    } else {
        printf("xrandr is not active\n");
        section = getActiveScreen(display, &root_window);
    }

    if (!section.valid) {
        printf("Couldn't get section\n");
        return 1;
    }

    printf("Section dimensions %dx%d\n", section.width, section.height);

    des_start_debug("Get XImage");
    XImage *image = XGetImage(display, root_window, section.x, section.y, section.width, section.height, AllPlanes, ZPixmap);

    des_end_debug("Get XImage");

    if (image == NULL) {
        printf("Could not get image\n");
        return 1;
    }

    des_start_debug("Swtich pixels");
    size_t memory_size = image->width * image->height * (image->bits_per_pixel * 4);
    uint *png_data = malloc(memory_size);

    if (image->byte_order == LSBFirst) {
        for (int row = 0; row < image->height; ++row) {
            for (int col = 0; col < image->width; ++col) {
                int index = (row * image->width) + col;
                uint pixel = (((uint *) image->data)[index]);

                uint r_shift = getShiftAmount(image->red_mask);
                uint g_shift = getShiftAmount(image->green_mask);
                uint b_shift = getShiftAmount(image->blue_mask);

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
    }
    des_end_debug("Swtich pixels");


    des_start_debug("Create Img");
    stbi_write_png("screenshot.png", image->width, image->height, 4, png_data, image->width * 4);
    des_end_debug("Create Img");

    des_print_all_debugs();
    free(png_data);
}
