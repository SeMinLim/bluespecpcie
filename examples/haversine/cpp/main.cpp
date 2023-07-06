#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "bdbmpcie.h"
#include "dmasplitter.h"


double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}


int main(int argc, char** argv) {
	BdbmPcie* pcie = BdbmPcie::getInstance();

	unsigned int d = pcie->readWord(0);
	printf( "Magic: %x\n", d );
	fflush( stdout );

	float seoul_lat = 37.547889;
	float seoul_lon = 126.997128;
	float busan_lat = 35.158874;
	float busan_lon = 129.043846;
	uint32_t seoul_lat_v = *(uint32_t*)&seoul_lat;
	uint32_t seoul_lon_v = *(uint32_t*)&seoul_lon;
	uint32_t busan_lat_v = *(uint32_t*)&busan_lat;
	uint32_t busan_lon_v = *(uint32_t*)&busan_lon;

	// Send data
	pcie->userWriteWord(0, seoul_lat_v);
	pcie->userWriteWord(4, seoul_lon_v);
	pcie->userWriteWord(8, busan_lat_v);
	pcie->userWriteWord(12, busan_lon_v);

	sleep(10);

	// Get result
	uint32_t result = pcie->userReadWord(0);
	uint32_t cycle = pcie->userReadWord(4);

	printf( "Result: %f\n", *(float*)&result );
	printf( "Total Cycles: %d\n", cycle );

	return 0;
}
