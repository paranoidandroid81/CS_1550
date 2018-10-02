/*
 * CS 1550 Project 1: Double-Buffered Graphics Library
 * Fall 2018
 * Author: Michael Korst (mpk44@pitt.edu)
 * Snake game to act as driver for graphics library
 */

#include "graphics.h"
#define UP 65             //macros to store ASCII arrow keys: A, B, C, D
#define DOWN 66
#define RIGHT 67
#define LEFT 68
#define MAX_X 639         //valid x between 0 and 639
#define MAX_Y 479         //valid y between 0 and 479

color_t snake_color = RGB(31, 33, 0);       //snake color set to orange
int curr_x = 0;         //x and y initial positions at (0,479) bottom left
int curr_y = MAX_Y;

void move_snake(void* img, int new_x, int new_y)
{
  if (new_x > MAX_X)          //snake too far right, wrap around
  {
    new_x = 0;
    curr_x = new_x;
  } else if (new_x < 0)       //snake too far left, wrap around
  {
    new_x = MAX_X;
    curr_x = new_x;
  }

  if (new_y > MAX_Y)        //snake too far down, wrap around
  {
    new_y = 0;
    curr_y = new_y;
  } else if (new_y < 0)     //snake too far down, wrap around
  {
    new_y = MAX_Y;
    curr_y = new_y;
  }
  //draw line between new points for snake movement
  draw_line(img, curr_x, curr_y, new_x, new_y, snake_color);
  curr_x = new_x;
  curr_y = new_y;
}

int main()
{
  int next_x = 0;         //where to move next based on arrow presses
  int next_y = 0;
  char key;               //user input
  char key2;              //for second char in ANSI sequence

  init_graphics();

  //new offscreen buffer to draw snake motion to
  void* buf = new_offscreen_buffer();

  draw_pixel(buf, curr_x, curr_y, snake_color);     //draw initial snake at (0,0)
  blit(buf);        //copy onto frame buffer

  do
  {
    //ANSI sequence
    next_x = curr_x;
    next_y = curr_y;          //reset location to current snake place
    key = getkey();       //get keypress
    key2 = getkey();      //get next keypress to check for ANSI arrow sequence
    if (key == 27 && key2 == 91)        //these values indicate arrow key presses, ESC and [
    {
      key = getkey();           //must call again to read actual arrow key code
      switch (key)
      {
        case UP:
          next_y--;
          break;
        case DOWN:
          next_y++;
          break;
        case RIGHT:
          next_x++;
          break;
        case LEFT:
          next_x--;
          break;
      }
      clear_screen(buf);                //clear screen
      move_snake(buf, next_x, next_y);      //draw new offscreen snake movement
      blit(buf);                        //copy offscreen to frame buffer
    } else if (key == 'q')
    {
      break;          //user wishes to quit
    }
    sleep_ms(200);            //sleep between frames of graphics
  }
  while (1);

  exit_graphics();        //unmap from memory, reset terminal settings
  return 0;
}
