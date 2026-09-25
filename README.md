# des_screenshot - Utility for X11

des_screenshot is a straightforward screenshot utility for X11 (Wayland is not supported) that supports mouse selection, clipboard copying, and screenshots of the active screen or window.

## Usage

Running the command will take a screenshot of the active screen (where the mouse is) and save it to the current working directory.

```sh
des_screenshot
```

To set the directory where the screenshot is saved, you can use the `--save-dir=` parameter (the folder must already exist).

```sh
des_screenshot --save-dir=/home/user/images/screenshots
```

To copy the screenshot to the clipboard, you can use `--clipboard` or simply `-c`.

```sh
des_screenshot --clipboard
```

To select a section of the screen to capture, you can use `--select` or `-s`. This will change your cursor. To start the selection, hold the left mouse button and drag until the rectangle covers the area you want to capture.

### `--help` output:

```sh
Usage: des_screenshot [OPTIONS...]
A list of options with a brief description is given below.

-h, --help               Displays help and exits.
    --screen             Takes a screenshot of the active screen; this is the default mode.
-w, --window             Takes a screenshot of the active window.
-s, --select             Allows mouse selection of the area to take a screenshot of.
-c, --clipboard          Saves the screenshot to the clipboard.
    --save-dir=          Directory to save the screenshot to; defaults to the current directory.
```

### Example config for i3

This is my personal i3 config:

```sh
# Starts des_screenshot in mouse selection mode and saves the screenshot to the /tmp/ folder and to the clipboard
bindsym Print --release exec --no-startup-id des_screenshot --select -c --save-dir=/tmp/

# Starts des_screenshot in active screen mode (default) and saves the screenshot to the /home/damian/Pictures/des_screenshots/ folder and to the clipboard
bindsym $mod+Print --release exec --no-startup-id des_screenshot -c --save-dir=/home/damian/Pictures/des_screenshots/

# Starts des_screenshot in active window mode and saves the screenshot to the /home/damian/Pictures/des_screenshots/ folder and to the clipboard
bindsym $mod+Shift+Print --release exec --no-startup-id des_screenshot --window -c --save-dir=/home/damian/Pictures/des_screenshots/
```

# Installation

You can download the precompiled binary from the [releases page](https://github.com/damianed/des_screenshot/releases/), or you can build it yourself by running the `build-release.sh` script located in the `build/` folder.

Then, copy the binary to a folder where you want it to live (e.g., `/usr/bin/`) and add the binary to your `PATH` if its containing folder isn't already in it.
