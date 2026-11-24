#include <sys/mman.h>
#include <stdio.h>

__attribute__((constructor))
static void bloquear(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("mlockall");
}
