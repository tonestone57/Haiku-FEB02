#include <locale.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {

	if (argc < 2) {
		printf("Usage: %s <string>\n", argv[0]);
		return 1;
	}

	setlocale(LC_CTYPE, "");

	printf("Len: %i\n", mblen(argv[1], SSIZE_MAX));

	return 0;
}
