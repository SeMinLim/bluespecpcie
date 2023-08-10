#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


// DBSCAN
#define CORE_POINT 1
#define BORDER_POINT 2

#define SUCCESS 0
#define UNCLASSIFIED -1
#define NOISE -2
#define FAILURE -3

#define MINIMUM_POINTS 2
#define EPSILON 8000.00

// Haversine
#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


typedef struct Point {
	float lat, lon; // latitude & longitude
	int quadID; 	// Quadrant Index
	int clusterID;  // clustered ID
}Point;

typedef struct Cluster {
	Point center;
	int startIdx;
	int endIdx;
	int numPoints;
	int goDbscan;
}Cluster;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmarkData(Point* data, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		fread(&data[i].lat, sizeof(float), 1, f_data);
		fread(&data[i].lon, sizeof(float), 1, f_data);
		data[i].clusterID = UNCLASSIFIED;
	}

	fclose(f_data);
}

// Haversine
float haversine(const Point pointCore, const Point pointTarget ) {
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

// Function for finding center mass value
void findCenterMass(Point* data, Cluster* cluster) {
	// Center Mass
	float totalX = 0.00;
	float totalY = 0.00;
	for ( int i = cluster->startIdx; i < cluster->endIdx; i ++ ) {
		totalX = totalX + data[i].lat;
		totalY = totalY + data[i].lon;
	}
	cluster->center.lat = totalX / cluster->numPoints;
	cluster->center.lon = totalY / cluster->numPoints;
}

// Function for initialization
void initialize(Point* data, int numPoints, Cluster* initial, Point* lowest, Point* highest ) {
	// Min and Max
	for ( int i = 0; i < numPoints; i ++ ) {
		if ( i == 0 ) {
			lowest->lat = data[i].lat;
			lowest->lon = data[i].lon;
			highest->lat = data[i].lat;
			highest->lon = data[i].lon;
		} else {
			if ( lowest->lat > data[i].lat ) lowest->lat = data[i].lat;
			if ( lowest->lon > data[i].lon ) lowest->lon = data[i].lon;
			if ( highest->lat < data[i].lat ) highest->lat = data[i].lat;
			if ( highest->lon < data[i].lon ) highest->lon = data[i].lon;
		}
	}

	// Initial cluster
	initial->startIdx = 0;
	initial->endIdx = numPoints;
	initial->numPoints = numPoints;
	initial->goDbscan = 1;

	// Center mass of dataset
	findCenterMass(data, initial);
}

// Quadtree
void quadtree(Point* data, Cluster* quadrants, int parentIdx) {
	// Initialization for the new children clusters
	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		quadrants[i].center.quadID = i;
		quadrants[i].center.clusterID = i;
		quadrants[i].numPoints = 0;
		quadrants[i].goDbscan = 1;
	}
	
	// Quadtree
	for ( int i = quadrants[parentIdx].startIdx; i < quadrants[parentIdx].endIdx; i ++ ) {
		if ( (data[i].lat < quadrants[parentIdx].center.lat) && (data[i].lon < quadrants[parentIdx].center.lon) ) {
			data[i].quadID = 4*parentIdx+1;
			quadrants[4*parentIdx+1].numPoints++;
		} else if ( (data[i].lat >= quadrants[parentIdx].center.lat) && (data[i].lon < quadrants[parentIdx].center.lon) ) {
			data[i].quadID = 4*parentIdx+2;
			quadrants[4*parentIdx+2].numPoints++;
		} else if ( (data[i].lat < quadrants[parentIdx].center.lat) && (data[i].lon >= quadrants[parentIdx].center.lon) ) {
			data[i].quadID = 4*parentIdx+3;
			quadrants[4*parentIdx+3].numPoints++;
		} else if ( (data[i].lat >= quadrants[parentIdx].center.lat) && (data[i].lon >= quadrants[parentIdx].center.lon) ) {
			data[i].quadID = 4*parentIdx+4;
			quadrants[4*parentIdx+4].numPoints++;
		}
	}

	// Data alignment
	Point temp;
	for ( int i = quadrants[parentIdx].startIdx; i < quadrants[parentIdx].endIdx; i ++ ) {
		for ( int j = quadrants[parentIdx].startIdx; j < quadrants[parentIdx].endIdx - 1; j ++ ) {
			if ( data[j].quadID > data[j+1].quadID ) {
				temp = data[j];
				data[j] = data[j+1];
				data[j+1] = temp;
			}
		}
	}

	// Set the value for the new clusters
	quadrants[4*parentIdx+1].startIdx = quadrants[parentIdx].startIdx;
	quadrants[4*parentIdx+1].endIdx = quadrants[4*parentIdx+1].startIdx + quadrants[4*parentIdx+1].numPoints;
	quadrants[4*parentIdx+2].startIdx = quadrants[4*parentIdx+1].endIdx;
	quadrants[4*parentIdx+2].endIdx = quadrants[4*parentIdx+2].startIdx + quadrants[4*parentIdx+2].numPoints;
	quadrants[4*parentIdx+3].startIdx = quadrants[4*parentIdx+2].endIdx;
	quadrants[4*parentIdx+3].endIdx = quadrants[4*parentIdx+3].startIdx + quadrants[4*parentIdx+3].numPoints;
	quadrants[4*parentIdx+4].startIdx = quadrants[4*parentIdx+3].endIdx;
	quadrants[4*parentIdx+4].endIdx = quadrants[4*parentIdx+4].startIdx + quadrants[4*parentIdx+4].numPoints;

	// Center mass of each quadrant
	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		if ( quadrants[i].numPoints >= 1 ) findCenterMass(data, &quadrants[i]);
		else quadrants[i].goDbscan = 0;
	}
	
}

int dbscanQuadrants(Point* data, Cluster* child, int parentIdx) {
	int cnt = 0;

	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		if ( child[i].goDbscan != 0 ) {
			for ( int j = i + 1; j < 4*parentIdx+5; j ++ ) {
				if ( child[j].goDbscan != 0 ) {
					if ( haversine(child[i].center, child[j].center) <= EPSILON ) {
						if ( child[j].center.quadID > child[i].center.quadID ) {
							child[j].center.quadID = child[i].center.quadID;
						} else {
							child[i].center.quadID = child[j].center.quadID;
						}
						cnt++;
					}
				}
			}
		}
	}

	return cnt;
}

void dbscanQuadtree(Point* data, Cluster* quadrants) {
	int clusterID = 1;
	int parentIdx = 0;
	int stopCondition = 3;
	while(1) {
		// Quadtree first
		quadtree(data, quadrants, parentIdx); 	

		// And then DBSCAN for the quadrants
		int cnt = dbscanQuadrants(data, quadrants, parentIdx);

		// Stop or Go
		if ( cnt == stopCondition ) {
			// Set the clusterID & Filter the noise
			for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
				if ( quadrants[i].center.quadID == quadrants[i].center.clusterID ) {
					for ( int j = quadrants[i].startIdx; j < quadrants[i].endIdx; j ++ ) {
						if ( haversine(quadrants[i].center, data[j]) <= EPSILON ) {
							data[j].clusterID = clusterID;
						} else data[j].clusterID = NOISE;
					}
					quadrants[i].center.clusterID = clusterID;
					clusterID++;
				} else {
					int idx = quadrants[i].center.quadID;
					int clusterIdx = quadrants[idx].center.clusterID;
					for ( int j = quadrants[i].startIdx; j < quadrants[i].endIdx; j ++ ) {
						if ( haversine(quadrants[i].center, data[j]) <= EPSILON ) {
							data[j].clusterID = clusterIdx;
						} else data[j].clusterID = NOISE;
					}
				}
			}
			break;
		}
	}
}

// Function for printing results
void printResults(Point* data, int numPoints) {
	int i = 0;

	printf("Number of points: %u\n"
	       " x     y     cluster_id\n"
	       "-----------------------\n", numPoints);

	while (i < numPoints) {
		printf("%f %f: %d\n", data[i].lat, data[i].lon, data[i].clusterID);
		++i;
	}
}


int main() {
	int numCities = 44691;

	Point* cities = (Point*)malloc(sizeof(Point)*numCities);
	Cluster* quadrants = (Cluster*)malloc(sizeof(Cluster)*numCities);

	// Read point data
	char benchmark_filename[] = "../worldcities.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);

	// Initialize
	Point lowest, highest;
	initialize(cities, numCities, &quadrants[0], &lowest, &highest);
	
	// DBSCAN with Quadtree
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStart = timeCheckerCPU();
	dbscanQuadtree(cities, quadrants);
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );
	
	// result of DBSCAN algorithm
	printResults(cities, numCities);    
	printf( "Elapsed Time (CPU): %.2f\n", processTime );

	free(cities);
	free(quadrants);

	return 0;
}
