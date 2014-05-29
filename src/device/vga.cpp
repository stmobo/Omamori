// most of this is derived from the tutorial on osdev.

#include "includes.h"
#include "lib/sync.h"
#include "device/vga.h"
#ifdef MIRROR_VGA_SERIAL
#include "device/serial.h"
#endif

static spinlock __vga_write_lock;

const size_t VGA_WIDTH = 80;
const size_t VGA_HEIGHT = 24;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
 
char make_color(enum vga_color fg, enum vga_color bg)
{
	return fg | bg << 4;
}
 
uint16_t make_vgaentry(char c, uint8_t color)
{
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

 
void terminal_initialize()
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = make_color(COLOR_LIGHT_GREY, COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for ( size_t y = 0; y < VGA_HEIGHT; y++ )
	{
		for ( size_t x = 0; x < VGA_WIDTH; x++ )
		{
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = make_vgaentry(' ', terminal_color);
		}
	}
}
 
void terminal_setcolor(char color)
{
	terminal_color = color;
}

// terminal_scroll - scroll the console
// Positive values scroll down (adding new lines to the bottom); negative values do the inverse.
void terminal_scroll(int num_rows)
{
    __vga_write_lock.lock();
    if(num_rows > 0) { // scroll down
        for(size_t y=0;y<VGA_HEIGHT-1;y++)
            for(size_t x=0;x<VGA_WIDTH;x++)
                terminal_buffer[y*VGA_WIDTH+x] = terminal_buffer[(y+1)*VGA_WIDTH+x];
        for(size_t x=0;x<VGA_WIDTH;x++)
            terminal_buffer[(VGA_HEIGHT-1)*VGA_WIDTH+x] = make_vgaentry(' ', make_color(COLOR_LIGHT_GREY, COLOR_BLACK));
    } else if(num_rows < 0) { // scroll up
        for(size_t y=VGA_HEIGHT-1;y>0;y--)
            for(size_t x=0;x<VGA_WIDTH;x++)
                terminal_buffer[y*VGA_WIDTH+x] = terminal_buffer[(y-1)*VGA_WIDTH+x];
        for(size_t x=0;x<VGA_WIDTH;x++)
            terminal_buffer[x] = make_vgaentry(' ', make_color(COLOR_LIGHT_GREY, COLOR_BLACK));
    }
    __vga_write_lock.unlock();
}
 
// terminal_putentryat - write a character to screen, with color and position attributes.
// This function directly modifies the terminal buffer.
void terminal_putentryat(char c, char color, size_t x, size_t y)
{
    //__vga_write_lock.lock();
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = make_vgaentry(c, color);
    //__vga_write_lock.unlock();
}

// terminal_putchar - write a single character to screen
// This function prints a character to screen in a manner similar to "terminal_writestring" (see below).
// '\n' characters are automatically used to scroll and start new lines.
void terminal_putchar(char c)
{
    __vga_write_lock.lock();
    if(c=='\n') {
        terminal_column = 0;
        if ( ++terminal_row == VGA_HEIGHT ) {
            terminal_scroll(1);
            terminal_row = VGA_HEIGHT-1;
        }
        __vga_write_lock.unlock();
        return;
    }
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if ( ++terminal_column == VGA_WIDTH )
	{
		terminal_column = 0;
		if ( ++terminal_row == VGA_HEIGHT )
		{
			terminal_scroll(1);
            terminal_row = VGA_HEIGHT-1;
		}
	}
    __vga_write_lock.unlock();
#ifdef MIRROR_VGA_SERIAL
    char d[2];
    d[0] = c;
    d[1] = '\0';
    if(serial_initialized) {
        serial_write(d);
    }// else {
    //    serial_print_basic(d);
    //}
#endif
}
 
// terminal_writestring - print a string to screen
// this function prints a line of text to screen, wrapping and scrolling if necessary.
void terminal_writestring(char* data)
{
    __vga_write_lock.lock();
	size_t datalen = strlen(data);
	for ( size_t i = 0; i < datalen; i++ ) {
		terminal_putchar(data[i]);
    }
    __vga_write_lock.unlock();
#ifdef MIRROR_VGA_SERIAL
    if(serial_initialized) {
        serial_write(data);
    }// else {
    //    serial_print_basic(data);
    //}
#endif
}

void terminal_writestring(char* data, size_t datalen)
{
	for ( size_t i = 0; i < datalen; i++ )
		terminal_putchar(data[i]);
}

void terminal_backspace() {
    __vga_write_lock.lock();
    if(--terminal_column > VGA_WIDTH) {
        if(--terminal_row > VGA_HEIGHT) {
            terminal_scroll(-1);
            terminal_row = 0;
        }
    }
    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
    __vga_write_lock.unlock();
}