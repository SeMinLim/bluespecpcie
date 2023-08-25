#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


typedef struct Point {
	float lat;
	float lon;
}Point;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmarkData(std::vector<Point> &cities, char* filename, int length) {
	FILE* f_data = fopen(filename, "rb");
	if (f_data == NULL ) {
		printf("File not found: %s\n", filename);
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		int numPoints = cities.size();
		cities.resize(numPoints+1);
		fread(&cities[i].lat, sizeof(float), 1, f_data);
		fread(&cities[i].lon, sizeof(float), 1, f_data);
	}

	fclose(f_data);
}

// Haversine
float haversine(const Point pointCore, const Point pointTarget) {
	// Distance between latitudes and longitudes
	float dlat = (pointTarget.lat-pointCore.lat)*TO_RADIAN;
	float dlon = (pointTarget.lon-pointCore.lon)*TO_RADIAN;

	// Convert to radians
	float rad_lat_core = pointCore.lat*TO_RADIAN;
	float rad_lat_target = pointTarget.lat*TO_RADIAN;

	// Apply formula
	float f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(rad_lat_core) * cos(rad_lat_target);
	return asin(sqrt(f)) * 2 * EARTH_RADIUS;
}

int main(int argc, char **argv)
{
	int numCities = 44691;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);
	
	// Haversine formula
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 39899942; i ++ ) {
		haversine(cities[0], cities[1]);
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.8f\n", processTime );
	
	return 0;
}
