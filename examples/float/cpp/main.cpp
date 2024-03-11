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

	// 1st: floating-point addition, subtraction, multiplication, and division
	float x = 10.00;
	float y = 5.00;
	uint32_t xv = *(uint32_t*)&x;
	uint32_t yv = *(uint32_t*)&y;

	pcie->userWriteWord(0, xv);
	pcie->userWriteWord(4, yv);

	sleep(1);
	
	printf( "Floating-Point Operation Result\n" );
	printf( "x:10.00, y:5.00\n" );
	uint32_t a = pcie->userReadWord(0);
	printf( "addition:       %f\n", *(float*)&a );
	uint32_t s = pcie->userReadWord(4);
	printf( "subtraction:    %f\n", *(float*)&s );
	uint32_t m = pcie->userReadWord(8);
	printf( "multiplication: %f\n", *(float*)&m );
	uint32_t d = pcie->userReadWord(12);
	printf( "division:       %f\n", *(float*)&d );
	uint32_t r = pcie->userReadWord(16);
	printf( "Squareroot:     %f\n", *(float*)&r );
	uint32_t c = pcie->userReadWord(20);
	printf( "comparison:     %f\n", *(float*)&c );

	// 2nd: type converter from unsigned 32-bit integer to float
	uint32_t u = 49;

	pcie->userWriteWord(8, u);

	sleep(1);

	printf( "Type Conversion Result (Unsigned 32-bit Integer to Float)\n" );
	printf( "Unsigned 32-bit Integer: %d\n", u );
	uint32_t f = pcie->userReadWord(24);
	printf( "Float:                   %f\n", *(float*)&f );

	return 0;
}
