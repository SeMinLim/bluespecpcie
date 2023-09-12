#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


#define EPSILON 0.97


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
	
	float mag_x = sqrt(pow(pointCore.lat, 2) + pow(pointCore.lon, 2));
	float mag_y = sqrt(pow(pointTarget.lat, 2) + pow(pointTarget.lon, 2));
	float mag_xy = mag_x * mag_y;

	return xy / mag_xy;
}

Point inverseCosineSimilarityLat(Point pointCore) {
	Point result;
	
	// a
	float cs2 = pow(EPSILON, 2);
	float coreLat2 = pow(pointCore.lat, 2);
	float coreLon2 = pow(pointCore.lon, 2);
	float a = (cs2*coreLat2) + (cs2*coreLon2) - coreLat2;

	// b
	float b = -2 * pointCore.lat * coreLon2;

	// c
	float coreLon4 = pow(coreLon2, 2);
	float c = (cs2*coreLat2*coreLon2) + (cs2*coreLon4) - coreLon4;

	// Result
	result.lat = ((-1*b) + sqrt(pow(b, 2) - (4*a*c))) / (2*a);
	result.lon = ((-1*b) - sqrt(pow(b, 2) - (4*a*c))) / (2*a);
	
	return result;
}

Point inverseCosineSimilarityLon(Point pointCore) {
	Point result;
	
	// a
	float cs2 = pow(EPSILON, 2);
	float coreLat2 = pow(pointCore.lat, 2);
	float coreLon2 = pow(pointCore.lon, 2);
	float a = (cs2*coreLat2) + (cs2*coreLon2) - coreLon2;

	// b
	float b = -2 * pointCore.lon * coreLat2;

	// c
	float coreLat4 = pow(coreLat2, 2);
	float c = (cs2*coreLat2*coreLon2) + (cs2*coreLat4) - coreLat4;

	// Result
	result.lat = ((-1*b) + sqrt(pow(b, 2) - (4*a*c))) / (2*a);
	result.lon = ((-1*b) - sqrt(pow(b, 2) - (4*a*c))) / (2*a);
	
	return result;
}

int main(int argc, char **argv)
{
	int numCities = 700968;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities_augmented.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);
	
	// Inverse cosine similarity checking
	Point tempLat;
	Point tempLon;
	Point point1 = inverseCosineSimilarityLat(cities[0]);
	tempLat.lat = point1.lon;
	tempLat.lon = cities[0].lon;
	float cs1 = cosineSimilarity(cities[0], tempLat);
	printf( "%f, %f\n", cities[0].lat, cities[0].lon );
	printf( "%f, %f\n", point1.lat, tempLat.lon );
	printf( "%f, %f\n", point1.lon, tempLat.lon );
	printf( "%f\n", cs1 );

	Point point2 = inverseCosineSimilarityLon(cities[0]);
	tempLon.lat = cities[0].lat;
	tempLon.lon = point2.lon;
	float cs2 = cosineSimilarity(cities[0], tempLon);
	printf( "%f, %f\n", cities[0].lat, cities[0].lon );
	printf( "%f, %f\n", tempLon.lat, point2.lat );
	printf( "%f, %f\n", tempLon.lat, point2.lon );
	printf( "%f\n", cs2 );

	// Cosine similarity
	// Quadtree-based DBSCAN
	double processStart = timeCheckerCPU();
	for ( int i = 0; i < 147; i ++ ) {
		for ( int j = 0; j < 44691; j ++ ) {
			cosineSimilarity(cities[0], cities[j]);
		}
	}
	for ( int i = 0; i < 10429; i ++ ) {
		cosineSimilarity(cities[0], cities[i]);
	}
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Elapsed Time (CPU): %.8f\n", processTime );
	
	return 0;
}
