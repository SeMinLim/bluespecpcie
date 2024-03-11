#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "bdbmpcie.h"
#include "dmasplitter.h"


// Elapsed time checker
double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}


// Main
int main(int argc, char** argv) {
	BdbmPcie* pcie = BdbmPcie::getInstance();
	uint8_t* dmabuf = (uint8_t*)pcie->dmaBuffer();

	unsigned int d = pcie->readWord(0);
	printf( "Magic: %x\n", d );
	fflush( stdout );

	// Send the system start signal
	pcie->userWriteWord(0, 0);

	// Wait until the system is finished
	for ( uint32_t i = 0; i < 1024; i ++ ) {
		pcie->userWriteWord(4, 0);
	}

	// Get the values from HW
	for ( uint32_t i = 0; i < 50; i ++ ) {
		uint32_t f = pcie->userReadWord(4);
		printf( "Random Number (FP): %f\n", *(float*)&f );
	}

	return 0;
}
