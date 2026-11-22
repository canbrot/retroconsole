#include <sys/mman.h>
#include <stdio.h>

/* Se ejecuta al cargar la biblioteca, antes de main() */
__attribute__((constructor))
static void bloquear(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        perror("mlockall");
}
