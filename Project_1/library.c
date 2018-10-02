/*
 * CS 1550 Project 1: Double-Buffered Graphics Library
 * Fall 2018
 * Author: Michael Korst (mpk44@pitt.edu)
 */

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "graphics.h"

int fb_desc;          //frame buffer file descriptor
void* fb_mem;    //pointer to fb in memory
int screen_size;       //size of mmapped fb indicating size of display
struct fb_var_screeninfo virt_res;     //stores virtual resolution struct
struct fb_fix_screeninfo bit_depth;    //stores bit depth struct
struct termios term_settings;         //stores terminal settings


void init_graphics()
{
  fb_desc = open("/dev/fb0", O_RDWR);    //retrieve fb descriptor
  //retrieves and stores structs for virtual res and bit depth
  ioctl(fb_desc, FBIOGET_VSCREENINFO, &virt_res);
  ioctl(fb_desc, FBIOGET_FSCREENINFO, &bit_depth);
  //calculate total size of mmapped file from struct fields
  screen_size = virt_res.yres_virtual * bit_depth.line_length;
  //maps frame buffer in memory, stores pointer to address space
  fb_mem = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_desc, 0);
  //clear out the terminal screen for new frame Buffer, write clear command to std out 4 bytes
  write(STDOUT_FILENO, "\033[2J", 4);
  write(0, "\033[?25l", 7);               //remove cursor
  ioctl(STDIN_FILENO, TCGETS, &term_settings);   //retrive curr term term_settings
  term_settings.c_lflag &= ~ICANON;             //disable canonical mode
  term_settings.c_lflag &= ~ECHO;             //disable echo
  ioctl(STDIN_FILENO, TCSETS, &term_settings);   //passes new term term_settings
}

void exit_graphics()
{
  write(STDOUT_FILENO, "\033[2J", 4);    //clear term at exit
  write(0, "\033[?25h", 7);               //re-enable cursor
  munmap(fb_mem, screen_size);    //unmap frame buffer from memory
  close(fb_desc);         //close frame buffer descriptor
  term_settings.c_lflag |= ICANON;    //re-enable canonical mode
  term_settings.c_lflag |= ECHO;      //re-enable echo
  ioctl(STDIN_FILENO, TCSETS, &term_settings);    //pass reset term settings
}

//function that waits keypress and reads if it arrives
char getkey()
{
  char key;       //character to return
  fd_set fds;     //set of all open file descriptors, pass to select()
  FD_ZERO(&fds);      //clear fd set to ensure accurate reading
  FD_SET(STDIN_FILENO, &fds);     //add stdin to descriptor set to listen

  //if there is typing, read and store a byte in key char
  if (select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL) > 0)
  {
    read(STDIN_FILENO, &key, 1);
  }
  return key;
}

void sleep_ms(long ms)
{
  struct timespec sleep_time;     //struct to hold sleep time
  sleep_time.tv_sec = (ms / 1000);    //sets number of seconds
  //max milliseconds is 999, must modulo ms by 1000 and convert to nanosleep
  sleep_time.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&sleep_time, NULL);
}

//copy over each byte with value 0 to blank buffer
void clear_screen(void* img)
{
  size_t i;
  //iterate through all bytes in buffer to blank
  for (i = 0; i < screen_size; i++)
  {
    *((char*)(img) + i) = 0; //must cast to char* to access byte-wise, dereference and set to 0
  }
}

void draw_pixel(void* img, int x, int y, color_t color)
{
  if (x < 0 || y < 0 || x > virt_res.xres_virtual || y > virt_res.yres_virtual)
  {
    return;       //pixel out of bounds, return as invalid
  }

  //calculate pixel offset  to use in pointer arithmetic
  int offset = (y * virt_res.xres_virtual) + x;
  color_t* pixel = (color_t*)img + offset;     //find pixel in memory
  *pixel = color;     //set color
}

//employ Bresenham's algorithm to draw line with draw_pixel()
// used http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.616.2235&rep=rep1&type=pdf
void draw_line(void* img, int x1, int y1, int x2, int y2, color_t c)
{
  int delta_x = x2 - x1;
  int delta_y = y2 - y1;
  int curr_x = x1;					//start at (x1,y1), move towards (x2,y2)
  int curr_y = y1;
  int x_inc = 1;					//x and/or incremented only by 1 at a time
  int y_inc = 1;
  int two_dx = delta_x * 2;
  int two_dy = delta_y * 2;
  int two_dx_error, two_dy_error;

 	if (delta_x < 0)
 	{
 		x_inc = -1;							//decrement x if x going down
 		delta_x *= -1;
 		two_dx *= -1;
 	}

 	if (delta_y < 0)						//delta_y >= 0
 	{
 		y_inc = -1;
 		delta_y *= -1;
 		two_dy *= -1;
 	}

 	draw_pixel(img, x1, y1, c);							//always draw 1st point

 	if (delta_x != 0 || delta_y != 0)					//other points on the line?
 	{
 		if (delta_y <= delta_x)								//slope <= 1?
 		{
 			two_dx_error = 0;
 			while (curr_x != x2)
 			{
 				curr_x += x_inc;					//x's from x1 to x2
 				two_dx_error += two_dy;
 				if (two_dx_error > delta_x)
 				{
 					curr_y += y_inc;
 					two_dx_error -= two_dx;
 				}
 				draw_pixel(img, curr_x, curr_y, c);
 			}
 		} else					//slope large, reverse roles of x & y
 		{
 			two_dy_error = 0;
 			while (curr_y != y2)
 			{
 				curr_y += y_inc;				//y's from y1 to y2
 				two_dy_error += two_dx;
 				if (two_dy_error > delta_y)
 				{
 					curr_x += x_inc;
 					two_dy_error -= two_dy;
 				}
 				draw_pixel(img, curr_x, curr_y, c);
 			}
 		}
 	}
}

void* new_offscreen_buffer()
{
	void* new_buf = mmap(NULL, screen_size, PROT_READ | PROT_WRITE,
											MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);					//allocate new frame buffer
	return new_buf;
}

//iterate through bytes of src and copy to frame buffer
void blit(void* src)
{
	size_t i;

	for (i = 0; i < screen_size; i++)
	{
		*((char*)fb_mem + i) = *((char*)src + i);			//must cast to char* to access byte-wise
	}
}
