#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  lcg_srand(42);
  printf("%d\n", lcg_rand());
  printf("%d\n", lcg_rand());
  printf("%d\n", lcg_rand());
  exit(0);
}
