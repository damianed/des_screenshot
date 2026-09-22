COMPILE_FILES="x11_main.c clipboard.c lib/des_string_view.c"
gcc -DDEBUG -g -Wall -Wextra -Werror -std=gnu99 -Wvla -pedantic $COMPILE_FILES -o screenshot -lX11 -lXrandr
