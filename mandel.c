#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

#define ITERATION_DEPTH 1000

struct Complex {
  double r;
  double c;
};

int ws_row;
int ws_col;
int buffer_size;
// TODO devide cell into 10 cells and differentiate char based on density
const char cell_occupied = '#';
const char cell_empty = ' ';

const size_t convergence_cutoff = ITERATION_DEPTH / 50;

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

bool map_converges(struct Complex c)
{
  struct Complex z = c_zero;
  struct Complex convergence_sample[convergence_cutoff];
  for (size_t n = 0; n < ITERATION_DEPTH; ++n)
  {
    z = map(z, c);
    const size_t index = n % convergence_cutoff;
    convergence_sample[index] = z;
    if (n >= convergence_cutoff && sample_converges(convergence_sample, index)) {
      return true;
    }
  }
  return false;
}

struct Complex cell_coordinate(size_t frame_index, double origin_x, double origin_y, size_t zoom)
{
  const struct Complex origin = {origin_x, origin_y};
  const double step_size =  0.05 / (zoom + 1);
  const int y = frame_index / ws_col;
  const int x = frame_index - y * ws_col;
  const int relative_x = x - ws_col / 2;
  const int relative_y = y - ws_row / 2;
  const struct Complex coordinate = {origin.r + relative_x * step_size, origin.c + relative_y * step_size};
  return coordinate;
}

void update_frame(char frame[], double origin_x, double origin_y, size_t zoom)
{
  for (int i = 0; i < buffer_size; ++i) {
    if (map_converges(cell_coordinate(i, origin_x, origin_y, zoom))) {
      frame[i] = cell_occupied;
    } else {
      frame[i] = cell_empty;
    }
  }
}

int main(int argc, char *argv[])
{
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  ws_col = w.ws_col;
  // subtracting by one because last line is empty because of cursor
  ws_row = w.ws_row - 1;
  buffer_size = ws_col * ws_row;
  char frame[buffer_size];
  clear_screen();
  // TODO make screen not static
  update_frame(frame, 0, -1, 2);
  draw_frame(frame);
  return 0;
}
