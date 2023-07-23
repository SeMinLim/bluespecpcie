#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


float cities[44691][2];
float result[44691];


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmark() {
	FILE *data;
	data = fopen("worldcities.csv", "ra");

	int i = 0;

	while (i < 44691) {
		fscanf(data, "%f,%f\n", &(cities[i][0]), &(cities[i][1]));
		i++;
	}

	fclose(data);
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


int main()
{
	int i = 0;

	readBenchmark();
	printf( "Read Benchmark File Done!\n" );

	double processStart = timeCheckerCPU();
	while ( i < 10 ) {
		result[i] = haversine(cities[8][0], cities[8][1], cities[i][0], cities[i][1]);
		i++;
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.2f\n", processTime );
	
	// Seoul vs Seoul
	// The result is gonna be 0km and mi
	printf("dist: %.4f km (%.4f mi.)\n", result[8], (result[8]/1.609344));

	return 0;
}
