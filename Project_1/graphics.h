/*
 * CS 1550 Project 1: Double-Buffered Graphics Library
 * Fall 2018
 * Author: Michael Korst (mpk44@pitt.edu)
 */

//macro to encode color in 16 bit value
#define RGB(r, g, b) ((color_t) ((r & 0x1f) << 11) | ((g & 0x3f) << 5) | (b & 0x1f))

  //typedef to make 16-bit unsigned val for color type
typedef unsigned short color_t;

void init_graphics();

void exit_graphics();

char getkey();

void sleep_ms(long ms);

void clear_screen(void* img);

void draw_pixel(void* img, int x, int y, color_t color);

void draw_line(void* img, int x1, int y1, int x2, int y2, color_t c);

void* new_offscreen_buffer();

void blit(void* src);
