#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int resize(void **pnt, int n, int *len, int size)
{
        if (n == *len)
        {
                int dummylen = (*len) == 0 ? 2 : (*len) * 2;
                void *dummypnt = (void *)realloc(*pnt, dummylen * size);

                if (!dummypnt)
                {
                        fprintf(stderr, "resize::realloct: Cannot (re-)allocate %d bytes of memory\n", dummylen * size);
                        return -1;
                }

		*len = dummylen;
		*pnt = dummypnt;
        }
	else if (n > *len || n<0 || *len<0)
	{
		fprintf(stderr, "resize: fatal memory corruption problem: n > len || n<0 || len<0!");
		exit(1);
	}

	return 0;
}
