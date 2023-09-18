#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "bdbmpcie.h"
#include "dmasplitter.h"


#define NumCities 44691
#define Dimension 2


// Elapsed time checker
double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

// Function for reading benchmark file
void readfromfile(float* data, char* filename, size_t length) {
	FILE* f_data = fopen(filename, "rb");
	if (f_data == NULL ) {
		printf("File not found: %s\n", filename);
		exit(1);
	}

	fread(data, sizeof(float), length, f_data);

	fclose(f_data);
}

// Main
int main(int argc, char** argv) {
	BdbmPcie* pcie = BdbmPcie::getInstance();

	unsigned int d = pcie->readWord(0);
	printf( "Magic: %x\n", d );
	fflush( stdout );

	char cities_filename[] = "worldcities.bin";
	float* cities_float = (float*)malloc(sizeof(float)*NumCities*Dimension);
	uint32_t* cities_uint32 = (uint32_t*)malloc(sizeof(float)*NumCities*Dimension);

	// Read benchmark file	
	readfromfile(&cities_float[0], cities_filename, NumCities*Dimension);

	// Change the type to uint32_t
	for ( int i = 0; i < NumCities*Dimension; i ++ ) {
		cities_uint32[i] = *(uint32_t*)&cities_float[i];
	}

	// Send data
	for ( int i = 0; i < 4; i ++ ) {
		pcie->userWriteWord(0, cities_uint32[16]);
		pcie->userWriteWord(4, cities_uint32[17]);
		pcie->userWriteWord(8, cities_uint32[16]);
		pcie->userWriteWord(12, cities_uint32[17]);
	}

	sleep(1);

	// Get result
	uint32_t result[4];
	uint32_t cycle[4];
	for ( int i = 0; i < 4; i ++ ) {
		result[i] = pcie->userReadWord(0);
		cycle[i] = pcie->userReadWord(4);
		printf( "Result: %f\n", *(float*)&result[i] );
		printf( "Cycle: %d\n", cycle[i] );
	}

	return 0;	
}
