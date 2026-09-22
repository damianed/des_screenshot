COMPILE_FILES="x11_main.c clipboard.c lib/des_string_view.c"
gcc -Ofast -g -Wall -Wextra -Werror -std=gnu99 -Wvla -pedantic $COMPILE_FILES -o des_screenshot -lX11 -lXrandr
