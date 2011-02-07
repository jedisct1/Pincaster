#include "common.h"
#include "utils.h"
#include <assert.h>

int main() {
  assert(0.0 != gc_distance_between_ellipsoidal_positions
	 (&(Position2D){.latitude = 48.510, .longitude = 2.240}, &(Position2D){.latitude = 48.512, .longitude = 2.243}));
  return 0;
}
