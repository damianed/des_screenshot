#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <dlfcn.h>
#include <unistd.h>
#include <time.h>
//TODO: remove this and load xrandr dynamically
#include <X11/extensions/Xrandr.h>

#include "clipboard.h"
#include "lib/des_string_view.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

#ifdef DEBUG
    #define DES_TIME_DEBUG_IMPLEMENTATION
    #include "lib/des_time_debug.h"
#endif

#define global_variable static
#define local_persist   static
#define internal        static

#define uint64 uint64_t
#define uint   uint32_t
#define uint8  uint8_t

global_variable char *IMAGE_PATH = "/tmp/x11_screenshooter_screenshot.png";

#define BORDERS_COUNT 4
global_variable Window BORDERS[BORDERS_COUNT];

typedef enum {
    MODE_ACTIVE_SCREEN,
    MODE_ACTIVE_WINDOW,
    MODE_MOUSE_SELECT,
    MODE_INVALID,
} Mode;

typedef struct {
    StringView save_dir;
    Mode mode;
    bool copy_to_clipboard;
} Options;

typedef struct {
    bool valid;
    int x, y;
    uint width, height;
} ScreenSection;

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

    XRRScreenResources *screens = XRRGetScreenResourcesCurrent(display, *root_window);

    if (!screens) {
        printf("Failed to get resources from xrandr\n");
        return result;
    }

    Window root_return, child_return;
    int root_x, root_y, win_x, win_y;
    uint mask_return;
    if (XQueryPointer(display, *root_window, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask_return)) {

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
    } else {
        printf("Failed to query pointer\n");
    }

    XRRFreeScreenResources(screens);

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

ScreenSection getActiveWindow(Display *display, Window *root_window) {
    //TODO: test this for child windows
    ScreenSection result = {0};

    Atom active_window_property = XInternAtom(display, "_NET_ACTIVE_WINDOW", 0);
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *data;

    XGetWindowProperty(
            display,
            *root_window,
            active_window_property,
            0, 1, 0,
            XA_WINDOW,
            &type_return,
            &format_return,
            &nitems_return,
            &bytes_after_return,
            &data
    );

    if (data == 0 || type_return != XA_WINDOW) {
        fprintf(stderr, "Couldn't get active window\n");
        return result;
    }

    Window window_return = *((Window *) data);

    XWindowAttributes attributes;
    XGetWindowAttributes(display, window_return, &attributes);
    Window child;
    XTranslateCoordinates(display, window_return, *root_window, 0, 0, &result.x, &result.y, &child);

    result.width = attributes.width;
    result.height = attributes.height;
    result.valid = 1;

    return result;
}

void initializeBorderWindows(Display *display, Window root, Window BORDERS[4], uint size) {
    Screen *screen = DefaultScreenOfDisplay(display);
    XSetWindowAttributes attr;
    attr.background_pixel = XWhitePixel(display, 0);
    attr.override_redirect = 1;
    Atom win_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", 0);
    Atom win_dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", 0);

    for (uint i = 0; i < size; i++) {
        BORDERS[i] = XCreateWindow(
                display, root,
                0, 0,
                WidthOfScreen(screen),
                HeightOfScreen(screen),
                0, CopyFromParent,
                InputOutput, CopyFromParent,
                CWOverrideRedirect | CWBackPixel,
                &attr
        );

        XChangeProperty(
                display, BORDERS[i],
                win_type, XA_ATOM,
                32, PropModeReplace,
                (unsigned char *)&win_dock, 1
        );
    }
}

void drawSelectionBorders(Display *display, Window root, int x, int y, int width, int height) {
    local_persist int borders_initialized = 0;
    if (!borders_initialized) {
        initializeBorderWindows(display, root, BORDERS, BORDERS_COUNT);
        borders_initialized = 1;
    }

    if (width == 0 || height == 0) {
        return;
    }

    uint line_width = 1;
    x -= line_width;
    y -= line_width;
    width += line_width;
    height += line_width;

    XRectangle rects[BORDERS_COUNT] = {
        //left
        {x, y + line_width,
        line_width, height - line_width},
        //top
        {x, y,
        width, line_width},
        //right
        {x + width, y,
        line_width, height},
        //bottom
        {x, y + height,
        width + line_width, line_width}
    };

    for (size_t i = 0; i < BORDERS_COUNT; i++) {
        XRectangle *rect = rects + i;
        XMoveResizeWindow(display, BORDERS[i], rect->x, rect->y, rect->width, rect->height);
        XMapWindow(display, BORDERS[i]);
    }
}

ScreenSection getMouseSelection(Display *display, Window *root_window) {
    ScreenSection result = {0};

    Cursor cursor = XCreateFontCursor(display, XC_crosshair);

    if (XGrabPointer(display, *root_window, 0, ButtonPressMask|ButtonReleaseMask|ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime)) {
        fprintf(stderr, "Couldn't grab pointer\n");
        XFreeCursor(display, cursor);
        exit(1);
    }

    if (XGrabKeyboard(display, *root_window, 0, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) {
        fprintf(stderr, "Couldn't grab keyboard\n");
        XFreeCursor(display, cursor);
        XUngrabPointer(display, CurrentTime);
        exit(1);
    }

    XGCValues gcval;
    gcval.foreground = XWhitePixel(display, 0);
    gcval.function = GXxor;
    gcval.background = XBlackPixel(display, 0);
    gcval.plane_mask = gcval.background ^ gcval.foreground;
    gcval.subwindow_mode = IncludeInferiors;

    unsigned long gc_flags = GCFunction | GCForeground | GCBackground | GCSubwindowMode;
    GC gc = XCreateGC(display, *root_window, gc_flags, &gcval);

    bool select_started = 0;
    int rect_x = 0, rect_y =0, rect_width = 0, rect_height = 0;
    XEvent e;

    while (1) {
        if (XPending(display)) {
            XNextEvent(display, &e);

            if (select_started && e.type == MotionNotify) {
                rect_x = result.x;
                rect_y = result.y;
                rect_width = e.xmotion.x - rect_x;
                rect_height = e.xmotion.y - rect_y;

                if (rect_width < 0) {
                    rect_x = e.xmotion.x;
                    rect_width = 0 - rect_width;
                }

                if (rect_height < 0) {
                    rect_y = e.xmotion.y;
                    rect_height = 0 - rect_height;
                }

                drawSelectionBorders(
                        display, *root_window,
                        rect_x, rect_y,
                        rect_width, rect_height
                );
            }

            if (!select_started && e.type == ButtonPress) {
                select_started = 1;

                Window root_return, child_return;
                int root_x, root_y, win_x, win_y;
                uint mask_return;
                if (XQueryPointer(display, *root_window, &root_return, &child_return, &root_x, &root_y, &win_x, &win_y, &mask_return)) {
                    result.x = root_x;
                    result.y = root_y;
                }
            }

            if (select_started && e.type == ButtonRelease) {
                result.x = rect_x;
                result.y = rect_y;
                result.width = rect_width;
                result.height = rect_height;
                result.valid = 1;
                break;
            }

            //TODO: change this to have a grace period, I added a shortcut with i3
            //and it seems to stop it because it send the keypress that triggers the command
            //so I need to add --release
            if (e.type == KeyPress) {
                printf("Key pressed canceling...\n");
                break;
            }
        }
    }

    XFreeCursor(display, cursor);
    XUngrabKeyboard(display, CurrentTime);
    XUngrabPointer(display, CurrentTime);
    XFreeGC(display, gc);
    for (int i = 0; i < BORDERS_COUNT; i++) {
        XDestroyWindow(display, BORDERS[i]);
    }

    return result;
}

Mode getMode(char *arg) {
    if (strEquals(arg, "--window")) {
        return MODE_ACTIVE_WINDOW;
    }
    if (strEquals(arg, "--select")) {
        return MODE_MOUSE_SELECT;
    }

    return MODE_INVALID;
}

void parseArgs(int argc, char *argv[], Options *options) {
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        if (strEquals(arg, "--window") || strEquals(arg, "-w")) {
            options->mode = MODE_ACTIVE_WINDOW;
        } else if (strEquals(arg, "--select") || strEquals(arg, "-s")) {
            options->mode = MODE_MOUSE_SELECT;
        } else if (strEquals(arg, "--cliplboard") || strEquals(arg, "-c")) {
            options->copy_to_clipboard = 1;
        } else {
            StringView sv_arg = strToStringView(arg);
            StringView prefix = strToStringView("--save-dir=");
            if (strViewStartsWith(&sv_arg, &prefix)) {
                //TODO: this could be faster with a trimchars(uint n) function
                //since I already know how long prefix is
                strViewSplit(&sv_arg, '=');
                if (sv_arg.size > 0) {
                    options->save_dir = sv_arg;
                } else {
                    //TODO: show wrong usage message
                }
            } else {
                printf("Ignoring invalid argument %s\n", arg);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    //TODO: use save_dir and copy_to_clipboard options instead of just storing them
    //Default options
    Options options = {strToStringView("./"), MODE_ACTIVE_SCREEN, 0};
    parseArgs(argc, argv, &options);

    Display *display = XOpenDisplay(NULL);

    if (display == NULL) {
        fprintf(stderr, "Could not open display\n");
        return 1;
    }

    Screen *screen = ScreenOfDisplay(display, DefaultScreen(display));
    int screen_index = XScreenNumberOfScreen(screen);
    Window root_window;
    root_window = RootWindow(display, screen_index);

    int event_base, error_base;
    ScreenSection section;

    if (options.mode == MODE_INVALID) {
        printf("Invalid capture mode, falling back to active screen mode\n");
        options.mode = MODE_ACTIVE_SCREEN;
    }

    switch (options.mode) {
        case MODE_MOUSE_SELECT: {
            section = getMouseSelection(display, &root_window);
        } break;
        case MODE_ACTIVE_WINDOW:
            section = getActiveWindow(display, &root_window);
            if (section.valid) {
                break;
            }
            printf("Active window failed falling back to active screen\n");
            /* FALLTHRU */
        case MODE_ACTIVE_SCREEN: {
            if (XRRQueryExtension(display, &event_base, &error_base)) {
                section = getActiveScreenFromXrandr(display, &root_window);
            } else {
                printf("xrandr is not active\n");
                section = getActiveScreen(display, &root_window);
            }
        } break;
        case MODE_INVALID: {break;}
    }

    if (!section.valid) {
        fprintf(stderr, "Couldn't get screenshot section\n");
        return 1;
    }

    printf("Section dimensions %dx%d\n", section.width, section.height);

    XImage *image = XGetImage(display, root_window, section.x, section.y, section.width, section.height, AllPlanes, ZPixmap);

    if (image == NULL) {
        fprintf(stderr, "Could not get image\n");
        return 1;
    }

    size_t memory_size = image->width * image->height * (image->bits_per_pixel * 4);
    uint *png_data = malloc(memory_size);
    if (png_data == NULL) {
        printf("Failed to allocate %zu bytes\n", memory_size);
        return 1;
    }

    if (image->byte_order == LSBFirst) {
        uint r_shift = getShiftAmount(image->red_mask);
        uint g_shift = getShiftAmount(image->green_mask);
        uint b_shift = getShiftAmount(image->blue_mask);

        for (int row = 0; row < image->height; ++row) {
            for (int col = 0; col < image->width; ++col) {
                int index = (row * image->width) + col;
                uint pixel = (((uint *) image->data)[index]);

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

    stbi_write_png(IMAGE_PATH, image->width, image->height, 4, png_data, image->width * 4);

    free(png_data);
    XDestroyImage(image);

    int pid = fork();
    if (pid == 0) {
        setUpClipboard(display, root_window, IMAGE_PATH);
    }
}
