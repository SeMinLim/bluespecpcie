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
	unsigned int z = pcie->readWord(0);
	printf( "Magic: %x\n", z );
	fflush( stdout );
/*
	// Can multiple PEs access one registers?
	// NO
	printf( "Can multiple PEs access one register?\n" );
	pcie->userWriteWord(0, 0);
	
	uint32_t a = pcie->userReadWord(0);
	if ( a == 1 ) printf( "Yes!\n" );
	else printf( "No!\n" );
*/
	// Is it possible to use a 2D vector?
	printf( "Is it possible to use a 2D vector?" );
	pcie->userWriteWord(4, 0);

	uint32_t b = pcie->userReadWord(4);
	if ( b == 1 ) printf( "Yes!\n" );
	else printf( "No!\n" );

	return 0;
}
