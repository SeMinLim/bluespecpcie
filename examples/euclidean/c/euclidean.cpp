#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#define EPSILON 1


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

// Euclidean
float euclidean(const Point pointCore, const Point pointTarget) {
	float sub_lat = pointCore.lat - pointTarget.lat;
	float sub_lon = pointCore.lon - pointTarget.lon;

	float before_sqrt = pow(sub_lat, 2.00) + pow(sub_lon, 2.00);

	return sqrt(before_sqrt);
}


int main(int argc, char **argv)
{
	int numCities = 44691;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities_augmented.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);

	// Inverse euclidean checking
	Point tempLat;
	tempLat.lat = cities[0].lat + EPSILON;
	tempLat.lon = cities[0].lon;
	float e1 = euclidean(cities[0], tempLat);
	printf( "%f\n", e1 );
	Point tempLon;
	tempLon.lat = cities[0].lat;
	tempLon.lon = cities[0].lon + EPSILON;
	float e2 = euclidean(cities[0], tempLon);
	printf( "%f\n", e2 );
	
	// Euclidean
	// Quadtree-based DBSCAN
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 951; i ++ ) {
		for ( int j = 0; j < 44691; j ++ ) {
			euclidean(cities[0], cities[j]);
		}
	}
	for ( int i = 0; i < 18024; i ++ ) {
		euclidean(cities[0], cities[i]);
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.8f\n", processTime );

	return 0;
}
