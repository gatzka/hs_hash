/*
 * Runs the usage example that CMake extracts from README.md or from the
 * comment at the top of hs_hash.h.
 */

#include <stdio.h>

int example(void *device);

int main(void)
{
	int device = 0;
	int ret = example(&device);

	if (ret != 0) {
		printf("example() returned %d\n", ret);
		return 1;
	}
	printf("example() succeeded\n");
	return 0;
}
