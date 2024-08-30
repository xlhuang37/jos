// buggy program - causes a divide by zero exception

#include <inc/lib.h>

int zero;

void
umain(int argc, char **argv)
{
	cprintf("gay\n");
	zero = 0;
	cprintf("%d", 1/zero);
	cprintf("gay\n");
	cprintf("1/0 is %08x!\n", 1/zero);
}

