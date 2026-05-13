#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  lcg_srand(42);
  
  printf("Random numbers:\n");
  for(int i = 0; i < 10; i++) {
    printf("%d\n", lcg_rand());
  }
  
  exit(0);
}
