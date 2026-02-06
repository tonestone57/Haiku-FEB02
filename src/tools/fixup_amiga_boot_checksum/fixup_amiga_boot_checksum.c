#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#define BB_SIZE (2*512)

int main(int argc, char **argv)
{
	int fd;
	unsigned int i;
	uint32_t sum;
	uint8_t bootblock[BB_SIZE];
	uint32_t *p = (uint32_t *)bootblock;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	if (read(fd, bootblock, BB_SIZE) < BB_SIZE) {
		perror("read");
		close(fd);
		return 1;
	}
	if (ntohl(p[0]) != 'DOS\0') {
		fprintf(stderr, "bad bootblock signature!\n");
		return 1;
	}
	p[1] = 0;
	for (sum = 0, i = 0; i < (BB_SIZE)/sizeof(uint32_t); i++) {
		uint32_t old = sum;
		// big endian
		sum += ntohl(*p++);
		// overflow
		if (sum < old)
			sum++;
	}
	sum = ~sum;
	//fprintf(stderr, "checksum: 0x%lx\n", (long)sum);
	// big endian
	((uint32_t *)bootblock)[1] = htonl(sum);

	if (lseek(fd, 0LL, SEEK_SET) == -1) {
		perror("lseek");
		close(fd);
		return 1;
	}

	if (write(fd, bootblock, BB_SIZE) < BB_SIZE) {
		perror("write");
		close(fd);
		return 1;
	}

	close(fd);
	return 0;
}
