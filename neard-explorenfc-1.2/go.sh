gcc -I/usr/include/neardal examples/basic.c `pkg-config --cflags glib-2.0` -std=c11 `pkg-config --cflags --libs libvlc` -lglib-2.0 -lneardal -o prog -g
./prog -k
