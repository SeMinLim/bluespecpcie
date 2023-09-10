#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)
#define TO_DEGREE (180 / 3.1415926536)
#define EPSILON 5


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

// Inverse haversine for latitude
float inverseHaversineLat(const Point pointCore) {
	float dlat_1km = 0.008992;
	return dlat_1km * EPSILON;
}

// Inverse haversine for longitude
float inverseHaversineLon(const Point pointCore) {
	float sinFunc = sin((EPSILON * TO_RADIAN) / (2 * EARTH_RADIUS * TO_RADIAN));
	float powFunc = pow(sinFunc, 2);
	float secLat = 1 / cos(pointCore.lat * TO_RADIAN);
	return (2 * asin(sqrt(powFunc * secLat * secLat))) * TO_DEGREE;
}


int main(int argc, char **argv)
{
	int numCities = 700968*160;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities_augmented.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);

	// Inverse Haversine
	Point newPoint;
	float dlat = inverseHaversineLat(cities[0]);	
	float dlon = inverseHaversineLon(cities[0]);
	newPoint.lat = cities[0].lat + dlat;
	newPoint.lon = cities[0].lon + dlon;
	float epsilon = haversine(cities[0], newPoint);
	printf( "Distance: %f\n", epsilon );

	// Haversine formula
	// Quadtree-based DBSCAN
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 413; i ++ ) {
		for ( int j = 0; j < 700968*160; j ++ ) {
			haversine(cities[0], cities[j]);
		}
	}
	for ( int i = 0; i < 42931018; i ++ ) {
		haversine(cities[0], cities[i]);
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.8f\n", processTime );
	
	return 0;
}
