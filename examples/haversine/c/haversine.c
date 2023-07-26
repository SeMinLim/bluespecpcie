#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
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

// Haversine formula
float haversine(float lat1, float lon1, float lat2, float lon2)
{
	// Distance between latitudes and longitudes
	float dlat = (lat2-lat1)*TO_RADIAN;
	float dlon = (lon2-lon1)*TO_RADIAN;

	// Convert to radians
	lat1 = lat1*TO_RADIAN;
	lat2 = lat2*TO_RADIAN;

	// Apply formula
	float f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(lat1) * cos(lat2);
	return asin(sqrt(f)) * 2 * EARTH_RADIUS;
}


int main(int argc, char **argv)
{
	int r = 0;
	size_t numCities = 44691;
	size_t dimension = 2;

	float* cities = (float*)malloc(sizeof(float)*numCities*dimension);
	float* result = (float*)malloc(sizeof(float)*numCities);

	// Read float numbers from file
	readfromfile(&cities[0], argv[1], numCities*dimension);
	printf( "Read Benchmark File Done!\n" );
	printf( "Seoul: %f, %f\n", cities[16], cities[17] );
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 20; i = i + 2 ) {
		result[r] = haversine(cities[16], cities[17], cities[i], cities[i+1]);
		r++;
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.2f\n", processTime );
	
	// Seoul vs Seoul
	// The result is gonna be 0km and mi
	printf("dist: %.4f km (%.4f mi.)\n", result[8], (result[8]/1.609344));

	return 0;
}
