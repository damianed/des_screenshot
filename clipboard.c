#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "clipboard.h"

struct ImageMemory {
    unsigned char *data;
    size_t         size;
};

static struct ImageMemory IMAGE = {0};

static void sendTargets(Display *display, XSelectionRequestEvent *request, Atom targets, Atom png) {
    Atom possibleTargets[] = {png, targets};
    XSelectionEvent res;

    XChangeProperty(display, request->requestor, request->property, XA_ATOM, 32, PropModeReplace, (unsigned char *) possibleTargets, sizeof(possibleTargets) / sizeof(Atom));

    res.type      = SelectionNotify;
    res.requestor = request->requestor;
    res.selection = request->selection;
    res.target    = request->target;
    res.property  = request->property;
    res.time      = request->time;

    XSendEvent(display, request->requestor, 1, NoEventMask, (XEvent *) &res);
}

static void sendPng(Display *display, char *image_path, XSelectionRequestEvent *request, Atom png) {
    XSelectionEvent res;
    if (IMAGE.data == NULL) {
        FILE *file = fopen(image_path, "rb");

        if (file == NULL) {
            fprintf(stderr, "Couldn't open image file\n");
            return;
        }

        if (fseek(file, 0, SEEK_END) == -1) {
            fprintf(stderr, "Couldn't find end of screenshot file\n");
            return;
        }

        long size = ftell(file);
        if (size == -1) {
            fprintf(stderr, "Couldn't find size of screenshot file\n");
            return;
        }

        if  (fseek(file, 0, SEEK_SET) == -1) {
            fprintf(stderr, "Couldn't reset the file pointer\n");
            return;
        }

        IMAGE.data = malloc(size);
        IMAGE.size = size;
        fread(IMAGE.data, 1, IMAGE.size, file);

        fclose(file);
    }

    XChangeProperty(display, request->requestor, request->property, png, 8, PropModeReplace, IMAGE.data, IMAGE.size);

    res.type      = SelectionNotify;
    res.requestor = request->requestor;
    res.selection = request->selection;
    res.target    = request->target;
    res.property  = request->property;
    res.time      = request->time;

    XSendEvent(display, request->requestor, 1, NoEventMask, (XEvent *) &res);
}

static void sendNo(Display *display, XSelectionRequestEvent *request) {
    XSelectionEvent res;

    char *atom_name = XGetAtomName(display, request->type);
    if  (atom_name) {
        XFree(atom_name);
    }

    res.type      = SelectionNotify;
    res.requestor = request->requestor;
    res.selection = request->selection;
    res.target    = request->target;
    res.property  = None;
    res.time      = request->time;

    XSendEvent(display, request->requestor, 1, NoEventMask, (XEvent *) &res);
}

void setUpClipboard(Display *display, Window root, char *image_path) {
    Window window  = XCreateSimpleWindow(display, root, -10, -10, 1, 1, 0, 0, 0);

    Atom clipboard = XInternAtom(display, "CLIPBOARD", 0);
    Atom targets   = XInternAtom(display, "TARGETS", 0);
    Atom png       = XInternAtom(display, "image/png", 0);

    XSetSelectionOwner(display, clipboard, window, CurrentTime);

    XSelectionRequestEvent request;
    while (1) {
        XEvent e;
        XNextEvent(display, &e);
        switch (e.type) {
            case SelectionClear: {
                exit(0);
            } break;

            case SelectionRequest: {
                request = e.xselectionrequest;
                if (request.target == targets) {
                    sendTargets(display, &request, targets, png);
                } else if (request.target == png) {
                    sendPng(display, image_path, &request, png);
                } else {
                    sendNo(display, &request);
                }
            }

        }
    }
}
