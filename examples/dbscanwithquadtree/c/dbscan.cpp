#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


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
	int clusterID; 	// Cluster ID
}Point;

typedef struct Cluster {
	std::vector<Point> cities;
	Point center;
	int quadID;
	int goDbscan;
	int goQuadtree;
}Cluster;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmarkData(std::vector<Cluster> &quadrants, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		int numPoints = quadrants[0].cities.size();
		quadrants[0].cities.resize(numPoints+1);
		fread(&quadrants[0].cities[i].lat, sizeof(float), 1, f_data);
		fread(&quadrants[0].cities[i].lon, sizeof(float), 1, f_data);
		quadrants[0].cities[i].clusterID = UNCLASSIFIED;
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
void findCenterMass(Cluster* quadrants) {
	// Center Mass
	float totalX = 0.00;
	float totalY = 0.00;
	for ( int i = 0; i < (int)quadrants->cities.size(); i ++ ) {
		totalX = totalX + quadrants->cities[i].lat;
		totalY = totalY + quadrants->cities[i].lon;
	}
	quadrants->center.lat = totalX / (float)quadrants->cities.size();
	quadrants->center.lon = totalY / (float)quadrants->cities.size();
}

// Function for initialization
void initialize(std::vector<Cluster> &quadrants, Point* lowest, Point* highest ) {
	// Min and Max
	for ( int i = 0; i < (int)quadrants[0].cities.size(); i ++ ) {
		if ( i == 0 ) {
			lowest->lat = quadrants[0].cities[i].lat;
			lowest->lon = quadrants[0].cities[i].lon;
			highest->lat = quadrants[0].cities[i].lat;
			highest->lon = quadrants[0].cities[i].lon;
		} else {
			if ( lowest->lat > quadrants[0].cities[i].lat ) lowest->lat = quadrants[0].cities[i].lat;
			if ( lowest->lon > quadrants[0].cities[i].lon ) lowest->lon = quadrants[0].cities[i].lon;
			if ( highest->lat < quadrants[0].cities[i].lat ) highest->lat = quadrants[0].cities[i].lat;
			if ( highest->lon < quadrants[0].cities[i].lon ) highest->lon = quadrants[0].cities[i].lon;
		}
	}

	// Initial cluster
	quadrants[0].goDbscan = 1;

	// Center mass of dataset
	findCenterMass(&quadrants[0]);
}

// Quadtree
void quadtree(std::vector<Cluster> &quadrants, int parentIdx) {
	// Initialization for the new children quadrants
	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		quadrants[i].quadID = i;
		quadrants[i].goDbscan = 1;
		quadrants[i].goQuadtree = 1;
	}
	
	// Quadtree
	for ( int i = 0; i < (int)quadrants[parentIdx].cities.size(); i ++ ) {
		if ( (quadrants[parentIdx].cities[i].lat < quadrants[parentIdx].center.lat) && 
		     (quadrants[parentIdx].cities[i].lon < quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[4*parentIdx+1].cities.size();
			quadrants[4*parentIdx+1].cities.resize(numPoints+1);
			quadrants[4*parentIdx+1].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat >= quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon < quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[4*parentIdx+2].cities.size();
			quadrants[4*parentIdx+2].cities.resize(numPoints+1);
			quadrants[4*parentIdx+2].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat < quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon >= quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[4*parentIdx+3].cities.size();
			quadrants[4*parentIdx+3].cities.resize(numPoints+1);
			quadrants[4*parentIdx+3].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat >= quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon >= quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[4*parentIdx+4].cities.size();
			quadrants[4*parentIdx+4].cities.resize(numPoints+1);
			quadrants[4*parentIdx+4].cities[numPoints] = quadrants[parentIdx].cities[i];
		}
	}

	// Center mass of each quadrant
	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		if ( quadrants[i].cities.size() >= 1 ) findCenterMass(&quadrants[i]);
		else quadrants[i].goDbscan = 0;
	}
	
}

int dbscanQuadrants(std::vector<Cluster> &quadrants, int parentIdx) {
	int cnt = 0;

	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		if ( quadrants[i].goDbscan != 0 ) {
			for ( int j = i + 1; j < 4*parentIdx+5; j ++ ) {
				if ( quadrants[j].goDbscan != 0 ) {
					if ( haversine(quadrants[i].center, quadrants[j].center) <= EPSILON ) {
						if ( quadrants[j].quadID > quadrants[i].quadID ) {
							quadrants[j].quadID = quadrants[i].quadID;
						} else {
							quadrants[i].quadID = quadrants[j].quadID;
						}
						quadrants[i].goQuadtree = 0;
						quadrants[j].goQuadtree = 0;
						cnt++;
					}
				}
			}
		}
	}

	return cnt;
}

void collisionDetection(std::vector<Cluster> &quadrants, int quadID, int cityIdx, int parentIdx) {
	int cnt = 0;
	for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
		if ( i != quadID ) {
			if ( haversine(quadrants[i].center, quadrants[quadID].cities[cityIdx]) <= EPSILON ) {
				int idx = quadrants[i].quadID;
				int clusterIdx = quadrants[idx].center.clusterID;
				if ( clusterIdx != UNCLASSIFIED ) {
					quadrants[quadID].cities[cityIdx].clusterID = clusterIdx;
				} else {
					quadrants[quadID].cities.erase(quadrants[quadID].cities.begin() + cityIdx);
					quadrants[i].cities.push_back(quadrants[quadID].cities[cityIdx]);
				}
				break;
			} else cnt++;
		}
	}

	if ( cnt == 3 ) {
		quadrants[quadID].cities[cityIdx].clusterID = NOISE;
		noise++;
	}
}

void dbscanQuadtree(std::vector<Cluster> &quadrants) {
	int clusterID = 1;
	int parentIdx = 0;
	int stopCondition = 3;
	while(1) {
		// Quadtree first
		quadtree(quadrants, parentIdx); 	

		// And then DBSCAN for the quadrants
		int cnt = dbscanQuadrants(quadrants, parentIdx);

		// Stop or Go
		if ( cnt == stopCondition ) {
			// Set the clusterID & Filter the noise
			for ( int i = 4*parentIdx+1; i < 4*parentIdx+5; i ++ ) {
				if ( quadrants[i].quadID == i ) {
					for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
						if ( haversine(quadrants[i].center, quadrants[i].cities[j]) <= EPSILON ) {
							quadrants[i].cities[j].clusterID = clusterID;
						} else collisionDetection(quadrants, i, j, parentIdx);
					}
					quadrants[i].center.clusterID = clusterID;
					clusterID++;
				} else {
					int idx = quadrants[i].quadID;
					int clusterIdx = quadrants[idx].center.clusterID;
					for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
						if ( haversine(quadrants[i].center, quadrants[i].cities[j]) <= EPSILON ) {
							quadrants[i].cities[j].clusterID = clusterIdx;
						} else collisionDetection(quadrants, i, j, parentIdx);
					}
					quadrants[i].center.clusterID = clusterIdx;
				}
			}
			break;
		}
	}
}

// Function for printing results
void printResults(std::vector<Cluster> &quadrants) {
	int numPoints = quadrants[0].cities.size();

	printf("Number of points: %d\n"
	       " x     y     cluster_id\n"
	       "-----------------------\n", numPoints);

	for ( int i = 1; i < (int)quadrants.size(); i ++ ) {
		if ( quadrants[i].goQuadtree == 0 ) {
			for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
				printf("%f, %f: %d\n", quadrants[i].cities[j].lat, quadrants[i].cities[j].lon, 
						       quadrants[i].cities[j].clusterID);
			}
		}
	}
}


int main() {
	int numCities = 44691;

	std::vector<Cluster> quadrants(5);

	// Read point data
	char benchmark_filename[] = "../worldcities.bin";
	readBenchmarkData(quadrants, benchmark_filename, numCities);

	// Initialize
	Point lowest, highest;
	initialize(quadrants, &lowest, &highest);
	
	// DBSCAN with Quadtree
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStart = timeCheckerCPU();
	dbscanQuadtree(quadrants);
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// result of DBSCAN algorithm
	printResults(quadrants);
	printf( "Elapsed Time (CPU): %.4f\n", processTime );

	return 0;
}
