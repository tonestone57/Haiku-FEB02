#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

uint8_t sector[512];

int main(int argc, char **argv)
{
	int fd, i;
	uint16_t sum;
	uint8_t *p = sector;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	if (read(fd, sector, 512-2) < 512-2) {
		perror("read");
		close(fd);
		return 1;
	}
	for (sum = 0, i = 0; i < (512-2)/2; i++) {
		uint16_t v;
		v = *p++ << 8;
		v += *p++;
		sum += v;
	}
	sum = 0x1234 - sum /*+ 1*/;
	//sum = 0xaa55;
	// big endian
	*p++ = (uint8_t)(sum >> 8);
	*p++ = (uint8_t)sum;
	//lseek(fd, 0LL, SEEK_SET);
	if (write(fd, &sector[512-2], 2) < 2) {
		perror("write");
		close(fd);
		return 1;
	}
	close(fd);
	return 0;
}
