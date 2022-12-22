#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define ITERATION_DEPTH 1000

struct Complex {
  double r;
  double c;
};

int ws_row;
int ws_col;
int buffer_size;
const char cell_empty = ' ';
const char *illumination_dict = "@$#*!=;:~-,.";

const size_t convergence_cutoff = ITERATION_DEPTH / 20;

const struct Complex c_zero = {0, 0};

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

bool complex_invalid(struct Complex c)
{
  return isinf(c.r) || isinf(c.c) || isnan(c.r) || isnan(c.c);
}

bool sample_converges(struct Complex sample[], size_t start)
{
  const double start_radius = norm_squared(sample[start]);
  for (size_t i = 0; i < convergence_cutoff; ++i) {
    struct Complex c = sample[(start + i) % convergence_cutoff];
    if (complex_invalid(c) || norm_squared(c) > start_radius) {
      return false;
    }
  }
  return true;
}

int count = 0;

size_t map_converges(struct Complex c)
{
  struct Complex z = c_zero;
  struct Complex convergence_sample[convergence_cutoff];
  for (size_t n = 0; n < ITERATION_DEPTH; ++n)
  {
    z = map(z, c);
    const size_t index = n % convergence_cutoff;
    convergence_sample[index] = z;
    if (n >= convergence_cutoff && sample_converges(convergence_sample, (index + 1) % convergence_cutoff)) {
      return n;
    }
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

void update_frame(char frame[], double origin_x, double origin_y, size_t zoom)
{
  const size_t illumincation_step_size = ITERATION_DEPTH / 12;
  for (int i = 0; i < buffer_size; ++i) {
    size_t convergence_count = map_converges(cell_coordinate(i, origin_x, origin_y, zoom));
    if (convergence_count > ITERATION_DEPTH) {
      frame[i] = cell_empty;
    } else {
      frame[i] = illumination_dict[convergence_count * 12 / ITERATION_DEPTH];
    }
  }
}

void handle_user_input(char input, double *origin_x, double *origin_y, size_t *zoom) {
  const double translation_default = 0.5;
  if (input == 'j')
  {
    *origin_y += scale_step_size(translation_default, *zoom);
  }
  else if (input == 'k')
  {
    *origin_y -= scale_step_size(translation_default, *zoom);
  }
  else if (input == 'h')
  {
    *origin_x -= scale_step_size(translation_default, *zoom);
  }
  else if (input == 'l')
  {
    *origin_x += scale_step_size(translation_default, *zoom);
  }
  else if (input == '+')
  {
    *zoom = *zoom + 1;
  }
  else if (input == '-')
  {
    if (*zoom != 0)
    {
      *zoom = *zoom - 1;
    }
  }
  else if (input == 'r')
  {
    *zoom = 0;
    *origin_x = 0;
    *origin_y = 0;
  }
}

int main(int argc, char *argv[])
{
  // ----------------------------------------------------------------------------------------------
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
  size_t zoom = 1;
  update_frame(frame, origin_x, origin_y, zoom);
  draw_frame(frame);
  bool quit = false;
  while (!quit)
  {
    const char input = getchar();
    if (input == 'q')
    {
      quit = true;
    }
    else {
      handle_user_input(input, &origin_x, &origin_y, &zoom);
    }
    update_frame(frame, origin_x, origin_y, zoom);
    draw_frame(frame);
  }
  // ----------------------------------------------------------------------------------------------
  /*restore the old settings*/
  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  // ----------------------------------------------------------------------------------------------
  return 0;
}
