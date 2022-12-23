#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define ITERATION_DEPTH 3000

struct Complex {
  double r;
  double c;
};

int ws_row;
int ws_col;
int buffer_size;

const size_t convergence_cutoff = ITERATION_DEPTH / 50;

void draw_frame(char data[])
{
  printf("\x1b[H");
  for (int i = 0; ws_row * ws_col + 1 > i; i++)
    putchar(i % ws_col ? data[i] : '\n');
}

void clear_screen()
{
  printf("\x1b[2J");
}

struct Complex map(struct Complex z, struct Complex c)
{
  struct Complex ret;
  ret.r = z.r * z.r - z.c * z.c + c.r;
  ret.c = 2 * z.r * z.c + c.c;
  return ret;
}

double norm_squared(struct Complex c)
{
  return c.r * c.r + c.c * c.c;
}

size_t map_converges(struct Complex c)
{
  struct Complex z = {0, 0};
  for (size_t n = 0; n < ITERATION_DEPTH; ++n) {
    z = map(z, c);
    if (norm_squared(z) > 4)
      return n;
  }
  return ITERATION_DEPTH + 1;
}

double scale_step_size(double step_size, double zoom)
{
  return step_size / pow(3., zoom);
}

struct Complex cell_coordinate(size_t frame_index, double origin_x, double origin_y, size_t zoom)
{
  const struct Complex origin = {origin_x, origin_y};
  const double step_size = scale_step_size(0.05, zoom);
  const int y = frame_index / ws_col;
  const int x = frame_index - y * ws_col;
  const int relative_x = x - ws_col / 2;
  const int relative_y = y - ws_row / 2;
  const struct Complex coordinate = {origin.r + relative_x * step_size, origin.c + relative_y * step_size};
  return coordinate;
}

char shade(size_t convergence_count, size_t convergence_min, size_t convergence_max)
{
  static const char *shade_dict = " .,-~:;=!*#$@";
  size_t shade_index = 0;
  if (convergence_max > convergence_min) {
    const size_t convergence_diff = convergence_max - convergence_min;
    shade_index = (convergence_count - convergence_min) * sizeof(shade_dict) / convergence_diff;
  }
  return shade_dict[shade_index];
}

void update_frame(char frame[], double origin_x, double origin_y, size_t zoom)
{
  size_t convergence_map[buffer_size];
  size_t convergence_max = 0;
  size_t convergence_min = SIZE_MAX;
  for (int i = 0; i < buffer_size; ++i) {
    convergence_map[i] = map_converges(cell_coordinate(i, origin_x, origin_y, zoom));
    if (convergence_map[i] < convergence_min)
      convergence_min = convergence_map[i];
    if (convergence_map[i] > convergence_max && convergence_map[i] < ITERATION_DEPTH + 1)
      convergence_max = convergence_map[i];
  }
  for (int i = 0; i < buffer_size; ++i)
    frame[i] = shade(convergence_map[i], convergence_min, convergence_max);
}

void handle_user_input(char input, double *origin_x, double *origin_y, size_t *zoom) {
  const double translation_default = 0.5;
  if (input == 'j')
    *origin_y += scale_step_size(translation_default, *zoom);
  else if (input == 'k')
    *origin_y -= scale_step_size(translation_default, *zoom);
  else if (input == 'h')
    *origin_x -= scale_step_size(translation_default, *zoom);
  else if (input == 'l')
    *origin_x += scale_step_size(translation_default, *zoom);
  else if (input == '+')
    *zoom = *zoom + 1;
  else if (input == '-') {
    if (*zoom != 0)
      *zoom = *zoom - 1;
  }
  else if (input == 'r') {
    *zoom = 0;
    *origin_x = 0;
    *origin_y = 0;
  } else if (input == 'c') {
    *zoom = 15;
    *origin_x = -0.7269;
    *origin_y = 0.1889;
  }
}

int main(int argc, char *argv[])
{
  // ----------------------------------------------------------------------------------------------
  // Force terminal to pass keystrokes to program without pressing Enter
  static struct termios oldt, newt;

  /*tcgetattr gets the parameters of the current terminal
  STDIN_FILENO will tell tcgetattr that it should write the settings
  of stdin to oldt*/
  tcgetattr(STDIN_FILENO, &oldt);
  /*now the settings will be copied*/
  newt = oldt;

  /*ICANON normally takes care that one line at a time will be processed
  that means it will return if it sees a "\n" or an EOF or an EOL*/
  newt.c_lflag &= ~(ICANON);          

  /*Those new settings will be set to STDIN
  TCSANOW tells tcsetattr to change attributes immediately. */
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  // ----------------------------------------------------------------------------------------------
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  ws_col = w.ws_col;
  // subtracting by one because last line is empty because of cursor
  ws_row = w.ws_row - 1;
  buffer_size = ws_col * ws_row;
  char frame[buffer_size];
  clear_screen();
  double origin_x = 0;
  double origin_y = 0;
  size_t zoom = 0;
  update_frame(frame, origin_x, origin_y, zoom);
  draw_frame(frame);
  bool quit = false;
  while (!quit) {
    const char input = getchar();
    if (input == 'q') {
      quit = true;
      break;
    } else
      handle_user_input(input, &origin_x, &origin_y, &zoom);
    update_frame(frame, origin_x, origin_y, zoom);
    draw_frame(frame);
  }
  // ----------------------------------------------------------------------------------------------
  /*restore the old settings*/
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  // ----------------------------------------------------------------------------------------------
  return 0;
}
