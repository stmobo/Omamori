#pragma once
#include "includes.h"
 
extern const size_t VGA_WIDTH;
extern const size_t VGA_HEIGHT;
 
extern size_t terminal_row;
extern size_t terminal_column;
extern uint8_t terminal_color;
extern uint16_t* terminal_buffer;
enum vga_color
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

extern char make_color(enum vga_color, enum vga_color);
extern void terminal_initialize();
extern void terminal_setcolor(char);
extern void terminal_scroll(int);
extern void terminal_putentryat(char, char, size_t, size_t);
extern void terminal_putchar(char);
extern void terminal_writestring(char*);
extern void terminal_writestring(char*, size_t);
extern void terminal_backspace();