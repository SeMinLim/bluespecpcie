#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


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

// Cosine Similarity
float cosineSimilarity(const Point pointCore, const Point pointTarget) {
	float xy = pointCore.lat*pointTarget.lat + pointCore.lon*pointTarget.lon;
	
	float mag_x = sqrt(pow(pointCore.lat, 2.00) + pow(pointCore.lon, 2.00));
	float mag_y = sqrt(pow(pointTarget.lat, 2.00) + pow(pointTarget.lon, 2.00));
	float mag_xy = mag_x * mag_y;

	return xy / mag_xy;
}

int main(int argc, char **argv)
{
	int numCities = 44691;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);
	
	// Haversine formula
	// Pure DBSCAN
	int pureDBSCAN = 1997285481;
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < pureDBSCAN/44691; i ++ ) {
		for ( int j = 0; j < 44691; j ++ ) {
			cosineSimilarity(cities[0], cities[j]);
		}
	}
	// Quadtree-based DBSCAN
	/*int quadtreeDBSCAN = 6580006;
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 147; i ++ ) {
		for ( int j = 0; j < 44691; j ++ ) {
			cosineSimilarity(cities[0], cities[j]);
		}
	}*/
	for ( int i = 0; i < 10429; i ++ ) {
		cosineSimilarity(cities[0], cities[i]);
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.8f\n", processTime );
	
	return 0;
}
