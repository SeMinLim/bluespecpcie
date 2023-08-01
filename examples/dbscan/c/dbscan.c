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

#define MINIMUM_POINTS 1     	// minimum number of cluster
#define EPSILON 10000.00	// distance for clustering, metre^2i

#define NUMPOINTS 10

// Haversine
#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


typedef struct Point {
	float lat, lon; // latitude & longitude
	int clusterID;  // clustered ID
}Point;


Point m_points[NUMPOINTS];
int clusterSeeds[NUMPOINTS];
int clusterNeighbours[NUMPOINTS];

uint16_t clusterSeedsSize = 0;
uint16_t clusterNeighboursSize = 0;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmarkData(char* filename) {
	FILE *data = fopen(filename, "rb");
	if( data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < NUMPOINTS; i ++ ) {
		fread(&m_points[i].lat, sizeof(float), 1, data);
		fread(&m_points[i].lon, sizeof(float), 1, data);
		m_points[i].clusterID = UNCLASSIFIED;
	}

	fclose(data);
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

// Border point finder for core point
void calculateClusterSeeds(Point point) {
	clusterSeedsSize = 0;
	
	for( int i = 0; i < NUMPOINTS; i ++ ) {
		if ( haversine(point, m_points[i]) <= EPSILON ) {
			clusterSeeds[clusterSeedsSize++] = i;
		}
	}
}

// Border point finder for border point
void calculateClusterNeighbours(Point point) {
	clusterNeighboursSize = 0;

	for( int i = 0; i < NUMPOINTS; i ++ ) {
		if ( haversine(point, m_points[i]) <= EPSILON ) {
			clusterNeighbours[clusterNeighboursSize++] = i;
		}
	}
}

// Cluster expander
int expandCluster(Point point, int clusterID) {    
	calculateClusterSeeds(point);
	
	if ( clusterSeedsSize < MINIMUM_POINTS ) {
		point.clusterID = NOISE;
		return FAILURE;
	} else {
		for( int i = 0; i < clusterSeedsSize; i ++ ) {
			int pointBorder = clusterSeeds[i];
			m_points[pointBorder].clusterID = clusterID;
		}
			
		for( int i = 0; i < clusterSeedsSize; i ++ ) {
			int pointBorder = clusterSeeds[i];
			if ( m_points[pointBorder].lat == point.lat &&
			     m_points[pointBorder].lon == point.lon ) {
				continue;
			} else {
				calculateClusterNeighbours(m_points[pointBorder]);
			
				if ( clusterNeighboursSize >= MINIMUM_POINTS ) {
					for ( int j = 0; j < clusterNeighboursSize; j ++ ) {
						int pointNeighbour = clusterNeighbours[j];
						if ( m_points[pointNeighbour].clusterID == UNCLASSIFIED || 
						     m_points[pointNeighbour].clusterID == NOISE ) {
							if ( m_points[pointNeighbour].clusterID == UNCLASSIFIED ) {
								clusterSeeds[clusterSeedsSize++] = pointNeighbour;
							}
							m_points[pointNeighbour].clusterID = clusterID;
						}
					}
				}
			}
		}
		return SUCCESS;
	}
}

// DBSCAN main
void run() {
	int clusterID = 1;
	for( int i = 0; i < NUMPOINTS; i ++ ) {
		if ( m_points[i].clusterID == UNCLASSIFIED ) {
			if ( expandCluster(m_points[i], clusterID) != FAILURE ) clusterID += 1;
		}
	}
}

// Function for printing results
void printResults() {
	int i = 0;

	printf("Number of points: %u\n"
	       " x     y     cluster_id\n"
	       "-----------------------\n", NUMPOINTS);

	while (i < NUMPOINTS) {
		printf("%5.2lf %5.2lf: %d\n", m_points[i].lat, m_points[i].lon, m_points[i].clusterID);
		++i;
	}
}

int main() {
	char benchmark_filename[] = "../worldcities.bin";

	// read point data
	printf( "Read Benchmark File Start!\n" );
	readBenchmarkData(benchmark_filename);
	printf( "Read Benchmark File Done!\n" );
	printf( "\n" );

	// main loop
	printf( "DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStart = timeCheckerCPU();
	run();
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );

	// result of DBSCAN algorithm
	printResults();    
	printf( "Elapsed Time (CPU): %.2f\n", processTime );

	return 0;
}
