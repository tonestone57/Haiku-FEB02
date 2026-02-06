/*
 * Copyright 2009, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2007, Marcus Overhagen. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#define EXIT_FAILURE 1


static void
print_usage(bool error)
{
	printf("\n");
	printf("create_image\n");
	printf("\n");
	printf("usage: create_image -i <imagesize> [-c] [-f] <file>\n");
	printf("       -i, --imagesize    size of raw partition image file\n");
	printf("       -f, --file         the raw partition image file\n");
	printf("       -c, --clear-image  set the image content to zero\n");
	exit(error ? EXIT_FAILURE : 0);
}


int
main(int argc, char *argv[])
{
	off_t imageSize = 0;
	const char *file = NULL;
	bool clearImage = false;

	while (1) {
		int c;
		static struct option long_options[] = {
			{"file", required_argument, 0, 'f'},
			{"clear-image", no_argument, 0, 'c'},
			{"help", no_argument, 0, 'h'},
			{"imagesize", required_argument, 0, 'i'},
			{0, 0, 0, 0}
		};

		opterr = 0; /* don't print errors */
		c = getopt_long(argc, argv, "+hi:cf:", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
			case 'h':
				print_usage(false);
				break;

			case 'i':
			{
				unsigned long long size = strtoull(optarg, NULL, 10);
				if (strchr(optarg, 'G') || strchr(optarg, 'g'))
					size *= 1024 * 1024 * 1024;
				else if (strchr(optarg, 'M') || strchr(optarg, 'm'))
					size *= 1024 * 1024;
				else if (strchr(optarg, 'K') || strchr(optarg, 'k'))
					size *= 1024;

				if (size > (unsigned long long)std::numeric_limits<off_t>::max()) {
					fprintf(stderr, "Error: image size too large\n");
					exit(EXIT_FAILURE);
				}
				imageSize = (off_t)size;
				break;
			}

			case 'f':
				file = optarg;
				break;

			case 'c':
				clearImage = true;
				break;

			default:
				print_usage(true);
		}
	}

	if (file == NULL && optind == argc - 1)
		file = argv[optind];

	if (!imageSize || !file)
		print_usage(true);

	if (imageSize < 0) {
		fprintf(stderr, "Error: invalid image size\n");
		exit(EXIT_FAILURE);
	}

	if (imageSize % 512) {
		fprintf(stderr, "Error: image size must be a multiple of 512 bytes\n");
		exit(EXIT_FAILURE);
	}

	int fd = open(file, O_RDWR | O_CREAT, 0666);
	if (fd < 0) {
		fprintf(stderr, "Error: couldn't open file %s (%s)\n", file,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	struct stat st;
	if (fstat(fd, &st) < 0) {
		fprintf(stderr, "Error: stat()ing file %s failed (%s)\n", file,
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!S_ISREG(st.st_mode) && !S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
		fprintf(stderr, "Error: type of file %s not supported\n", file);
		exit(EXIT_FAILURE);
	}

	if (S_ISREG(st.st_mode)) {
		// regular file -- use ftruncate() to resize it
		if ((clearImage && ftruncate(fd, 0) != 0)
			|| ftruncate(fd, imageSize) != 0) {
			fprintf(stderr, "Error: resizing file %s failed (%s)\n", file,
				strerror(errno));
			exit(EXIT_FAILURE);
		}
	} else {
		// some kind of device -- clear it manually, if we have to
		if (clearImage) {
			char buffer[1024 * 1024];
			memset(buffer, 0, sizeof(buffer));

			off_t totalWritten = 0;
			ssize_t written;
			while ((written = write(fd, buffer, sizeof(buffer))) > 0)
				totalWritten += written;

			// We expect ENOSPC when writing until the end of the device.
			// TODO: We should probably first determine the size of the device
			// and try to write only that much.
			if (written < 0 && errno != ENOSPC) {
				fprintf(stderr, "Error: writing to device file %s failed "
					"(%s)\n", file, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
	}

	close(fd);
	return 0;
}
