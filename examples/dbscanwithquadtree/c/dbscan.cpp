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
#define EPSILON 1000.00

// Haversine
#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


typedef struct Point {
	float lat, lon; // latitude & longitude
	int clusterID; 	// Cluster ID
}Point;

typedef struct Cluster {
	std::vector<Point> cities;
	Point lowest;
	Point highest;
	Point center;
	int quadID;
	int goQuadtree;
	int goDbscan;
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

// Function for finding lowest and highest points
void findLowestHighest(std::vector<Cluster> &quadrants, int quadrantIdx) {
	for ( int i = 0; i < (int)quadrants[quadrantIdx].cities.size(); i ++ ) {
		if ( i == 0 ) {
			quadrants[quadrantIdx].lowest.lat = quadrants[quadrantIdx].cities[i].lat;
			quadrants[quadrantIdx].lowest.lon = quadrants[quadrantIdx].cities[i].lon;
			quadrants[quadrantIdx].highest.lat = quadrants[quadrantIdx].cities[i].lat;
			quadrants[quadrantIdx].highest.lon = quadrants[quadrantIdx].cities[i].lon;
		} else {
			if ( quadrants[quadrantIdx].lowest.lat > quadrants[quadrantIdx].cities[i].lat ) {
				quadrants[quadrantIdx].lowest.lat = quadrants[quadrantIdx].cities[i].lat;
			}
			if ( quadrants[quadrantIdx].lowest.lon > quadrants[quadrantIdx].cities[i].lon ) {
				quadrants[quadrantIdx].lowest.lon = quadrants[quadrantIdx].cities[i].lon;
			}
			if ( quadrants[quadrantIdx].highest.lat < quadrants[quadrantIdx].cities[i].lat ) {
				quadrants[quadrantIdx].highest.lat = quadrants[quadrantIdx].cities[i].lat;
			}
			if ( quadrants[quadrantIdx].highest.lon < quadrants[quadrantIdx].cities[i].lon ) {
				quadrants[quadrantIdx].highest.lon = quadrants[quadrantIdx].cities[i].lon;
			}
		}
	}
}

// Function for finding center mass value
void findCenterMass(std::vector<Cluster> &quadrants, int quadrantIdx) {
	float totalX = 0.00;
	float totalY = 0.00;
	for ( int i = 0; i < (int)quadrants[quadrantIdx].cities.size(); i ++ ) {
		totalX = totalX + quadrants[quadrantIdx].cities[i].lat;
		totalY = totalY + quadrants[quadrantIdx].cities[i].lon;
	}
	quadrants[quadrantIdx].center.lat = totalX / (float)quadrants[quadrantIdx].cities.size();
	quadrants[quadrantIdx].center.lon = totalY / (float)quadrants[quadrantIdx].cities.size();
}

// Function for initialization
void initialize(std::vector<Cluster> &quadrants) {
	// Min and Max
	findLowestHighest(quadrants, 0);

	// Initial cluster
	quadrants[0].goQuadtree = 1;
	quadrants[0].goDbscan = 1;

	// Center mass of dataset
	findCenterMass(quadrants, 0);
}

// Quadtree
void quadtree(std::vector<Cluster> &quadrants, int parentIdx) {
	// Generate quadrants first
	int numQuadrants = (int)quadrants.size();
	quadrants.resize(numQuadrants+4);

	// Initialization for the new children quadrants
	int firstQuad = numQuadrants;
	int lastQuad = (int)quadrants.size();
	for ( int i = firstQuad; i < lastQuad; i ++ ) {
		quadrants[i].quadID = i;
		quadrants[i].goQuadtree = 1;
		quadrants[i].goDbscan = 1;
	}
	
	// Quadtree
	for ( int i = 0; i < (int)quadrants[parentIdx].cities.size(); i ++ ) {
		if ( (quadrants[parentIdx].cities[i].lat < quadrants[parentIdx].center.lat) && 
		     (quadrants[parentIdx].cities[i].lon < quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[firstQuad].cities.size();
			quadrants[firstQuad].cities.resize(numPoints+1);
			quadrants[firstQuad].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat >= quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon < quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[firstQuad+1].cities.size();
			quadrants[firstQuad+1].cities.resize(numPoints+1);
			quadrants[firstQuad+1].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat < quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon >= quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[firstQuad+2].cities.size();
			quadrants[firstQuad+2].cities.resize(numPoints+1);
			quadrants[firstQuad+2].cities[numPoints] = quadrants[parentIdx].cities[i];
		} else if ( (quadrants[parentIdx].cities[i].lat >= quadrants[parentIdx].center.lat) && 
			    (quadrants[parentIdx].cities[i].lon >= quadrants[parentIdx].center.lon) ) {
			int numPoints = quadrants[firstQuad+3].cities.size();
			quadrants[firstQuad+3].cities.resize(numPoints+1);
			quadrants[firstQuad+3].cities[numPoints] = quadrants[parentIdx].cities[i];
		}
	}

	// Lowest, highest, and center mass value of each quadrant
	for ( int i = firstQuad; i < lastQuad; i ++ ) {
		if ( quadrants[i].cities.size() >= 1 ) {
			findLowestHighest(quadrants, i);
			findCenterMass(quadrants, i);
		} else quadrants[i].goDbscan = 0;
	}
	
}

// DBSCAN for quadrants
int dbscanQuadrants(std::vector<Cluster> &quadrants, int parentIdx) {
	int firstQuad = (int)quadrants.size() - 4;
	int lastQuad = (int)quadrants.size();
	int done = 0;
	for ( int i = firstQuad; i < lastQuad; i ++ ) {
		if ( quadrants[i].goDbscan != 0 ) {
			for ( int j = i + 1; j < lastQuad; j ++ ) {
				if ( quadrants[j].goDbscan != 0 ) {
					float distanceQuad_i = haversine(quadrants[i].lowest, quadrants[i].highest);
					float distanceQuad_j = haversine(quadrants[j].lowest, quadrants[j].highest);
					float newEpsilon = EPSILON + distanceQuad_i + distanceQuad_j;
					if ( haversine(quadrants[i].center, quadrants[j].center) <= newEpsilon ) {
						if ( quadrants[j].quadID > quadrants[i].quadID ) {
							quadrants[j].quadID = quadrants[i].quadID;
						} else {
							quadrants[i].quadID = quadrants[j].quadID;
						}
						quadrants[i].goQuadtree = 0;
						quadrants[j].goQuadtree = 0;
					}
				}
			}
		}
	}

	for ( int i = firstQuad; i < lastQuad; i ++ ) {
		if ( quadrants[i].goQuadtree == 0 ) done++;
	}

	return done;
}

// DBSCAN for local data (border point finder of core point)
void borderFinderCore(std::vector<Cluster> &quadrants, int quadrantIdx, int cityIdx, std::vector<int> &bordersCore) {
	bordersCore.resize(0);

	for ( int i = 0; i < (int)quadrants[quadrantIdx].cities.size(); i ++ ) {
		if ( quadrants[quadrantIdx].cities[i].lat != quadrants[quadrantIdx].cities[cityIdx].lat &&
		     quadrants[quadrantIdx].cities[i].lon != quadrants[quadrantIdx].cities[cityIdx].lon ) {
			if ( haversine(quadrants[quadrantIdx].cities[i], quadrants[quadrantIdx].cities[cityIdx]) <= EPSILON ) {
				bordersCore.push_back(i);
			}
		}
	}
}

// DBSCAN for local data (border point finder of each border point)
void borderFinderBorder(std::vector<Cluster> &quadrants, int quadrantIdx, int cityIdx, std::vector<int> &bordersBorder) {
	bordersBorder.resize(0);

	for ( int i = 0; i < (int)quadrants[quadrantIdx].cities.size(); i ++ ) {
		if ( quadrants[quadrantIdx].cities[i].lat != quadrants[quadrantIdx].cities[cityIdx].lat &&
		     quadrants[quadrantIdx].cities[i].lon != quadrants[quadrantIdx].cities[cityIdx].lon ) {
			if ( haversine(quadrants[quadrantIdx].cities[i], quadrants[quadrantIdx].cities[cityIdx]) <= EPSILON) {
				bordersBorder.push_back(i);
			}
		}
	}
}

// DBSCAN for local data (cluster expander)
void clusterExpander(std::vector<Cluster> &quadrants, int quadrantIdx, int cityIdx, int clusterID) {
	std::vector<int> bordersCore;
	std::vector<int> bordersBorder;
	borderFinderCore(quadrants, quadrantIdx, cityIdx, bordersCore);

	if ( (int)bordersCore.size() < MINIMUM_POINTS ) {
		quadrants[quadrantIdx].cities[cityIdx].clusterID = NOISE;
	} else {
		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			quadrants[quadrantIdx].cities[borderPoint].clusterID = clusterID;
		}

		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			if ( quadrants[quadrantIdx].cities[borderPoint].lat == quadrants[quadrantIdx].cities[cityIdx].lat &&
			     quadrants[quadrantIdx].cities[borderPoint].lon == quadrants[quadrantIdx].cities[cityIdx].lon ) {
				continue;
			} else {
				borderFinderBorder(quadrants, quadrantIdx, borderPoint, bordersBorder);
				
				if ( (int)bordersBorder.size() >= MINIMUM_POINTS ) {
					for ( int j = 0; j < (int)bordersBorder.size(); j ++ ) {
						int neighbourPoint = bordersBorder[j];
						if ( quadrants[quadrantIdx].cities[neighbourPoint].clusterID == UNCLASSIFIED ||
						     quadrants[quadrantIdx].cities[neighbourPoint].clusterID == NOISE ) {
							if ( quadrants[quadrantIdx].cities[neighbourPoint].clusterID == UNCLASSIFIED ) {
								bordersCore.push_back(neighbourPoint);
							}
							quadrants[quadrantIdx].cities[neighbourPoint].clusterID = clusterID;
						}
					}
				}
			}
		}
	}
}

// DBSCAN for local data
void dbscanLocalData(std::vector<Cluster> &quadrants) {
	int clusterID = 1;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		if ( quadrants[i].goQuadtree == 0 ) {
			if ( quadrants[i].quadID == i ) {
				for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
					if ( quadrants[i].cities[j].clusterID == UNCLASSIFIED ) {
						clusterExpander(quadrants, i, j, clusterID);
					}
				}
				quadrants[i].center.clusterID = clusterID;
				clusterID++;
			} else {
				int idx = quadrants[i].quadID;
				int clusterIdx = quadrants[idx].center.clusterID;
				for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
					if ( quadrants[i].cities[j].clusterID == UNCLASSIFIED ) {
						clusterExpander(quadrants, i, j, clusterIdx);
					}
				}
				quadrants[i].center.clusterID = clusterIdx;
			}
		}
	}
}

void dbscanQuadtree(std::vector<Cluster> &quadrants) {
	// 
	int parentIdx = 0;
	// If the distance between the whole center mass values of the generated quadrants are less than new epsilone value,
	// then the system will be stopped
	int stopCnt = 0;
	int stopCondition = 4;
	// The number of quadrants where should be dealt with at certain phase 
	int quadCnt = 0;
	int quadCondition = 1;

	int q = 0;
	while(1) {
		// Main
		if ( quadrants[parentIdx].goQuadtree == 1 ) {
			// Quadtree first
			quadtree(quadrants, parentIdx); 	

			// And then DBSCAN for the quadrants
			q = dbscanQuadrants(quadrants, parentIdx);

			// Update conditions
			stopCnt = stopCnt + q;
			quadCnt++;
		}
		
		// Stop or Go
		if ( quadCnt == quadCondition ) {
			if ( stopCnt == stopCondition ) {
				// Set the clusterID & Filter the noise
				dbscanLocalData(quadrants);
				break;
			} else {
				quadCondition = (4*quadCondition) - stopCnt;
				stopCondition = 4*quadCondition;
				stopCnt = 0;

				parentIdx++;
			}
		} else {
			parentIdx++;
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

	std::vector<Cluster> quadrants(1);

	// Read point data
	char benchmark_filename[] = "../worldcities.bin";
	readBenchmarkData(quadrants, benchmark_filename, numCities);

	// Initialize
	initialize(quadrants);
	
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
