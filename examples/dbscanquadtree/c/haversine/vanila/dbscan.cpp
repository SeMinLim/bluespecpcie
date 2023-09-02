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

#define MINIMUM_POINTS 4
#define EPSILON 1300.00

// Haversine
#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)
#define TO_DEGREE (180 / 3.1415926536)


int numDataPoints = 0;
int numHaversine = 0;


typedef struct Point {
	float lat, lon;
}Point;

typedef struct PointQuadTree {
	Point point;
	int datasetID;
}PointQuadTree;

typedef struct PointDBSCAN {
	Point point;
	Point northEastern;
	Point northWestern;
	Point southEastern;
	Point southWestern;
	int clusterID;
}PointDBSCAN;

typedef struct Quadrant {
	std::vector<Quadrant> child;
	std::vector<PointQuadTree> cities;
	Point northEastern;
	Point northWestern;
	Point southEastern;
	Point southWestern;
	Point center;
	float diagonal;
	bool done;
}Quadrant;


// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
// Quadrant
void readBenchmarkDataQuadTree(Quadrant &root, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		int numPoints = root.cities.size();
		root.cities.resize(numPoints+1);
		fread(&root.cities[i].point.lat, sizeof(float), 1, f_data);
		fread(&root.cities[i].point.lon, sizeof(float), 1, f_data);
		root.cities[i].datasetID = i;
	}

	root.done = 0;
	
	fclose(f_data);
}
// Data points
void readBenchmarkDataDBSCAN(std::vector<PointDBSCAN> &dataset, char* filename, int length) {
	FILE *f_data = fopen(filename, "rb");
	if( f_data == NULL ) {
		printf( "File not found: %s\n", filename );
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		int numPoints = dataset.size();
		dataset.resize(numPoints+1);
		fread(&dataset[i].point.lat, sizeof(float), 1, f_data);
		fread(&dataset[i].point.lon, sizeof(float), 1, f_data);
		dataset[i].clusterID = UNCLASSIFIED;
	}
	
	fclose(f_data);
}

// Haversine
float haversine(const Point pointCore, const Point pointTarget) {
	numHaversine++;
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

// Function for four edge points of square
void findEdgePointsEpsilonBox(std::vector<PointDBSCAN> &dataset) {
	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		float dlat = inverseHaversineLat(dataset[i].point);
		float dlon = inverseHaversineLon(dataset[i].point);
		dataset[i].northEastern.lat = dataset[i].point.lat + dlat;
		dataset[i].northEastern.lon = dataset[i].point.lon + dlon;
		dataset[i].northWestern.lat = dataset[i].point.lat - dlat;
		dataset[i].northWestern.lon = dataset[i].point.lon + dlon;
		dataset[i].southEastern.lat = dataset[i].point.lat + dlat;
		dataset[i].southEastern.lon = dataset[i].point.lon - dlon;
		dataset[i].southWestern.lat = dataset[i].point.lat - dlat;
		dataset[i].southWestern.lon = dataset[i].point.lon - dlon;
	}
}

// Function for four edge points of quadrant
void findEdgePointsQuadrant(Quadrant &root) {
	// First child quad
	root.child[0].northEastern.lat = root.center.lat;
	root.child[0].northEastern.lon = root.center.lon;
	root.child[0].northWestern.lat = root.southWestern.lat;
	root.child[0].northWestern.lon = root.center.lon;
	root.child[0].southEastern.lat = root.center.lat;
	root.child[0].southEastern.lon = root.southWestern.lon;
	root.child[0].southWestern.lat = root.southWestern.lat;
	root.child[0].southWestern.lon = root.southWestern.lon;

	// Second child quad
	root.child[1].northEastern.lat = root.northEastern.lat;
	root.child[1].northEastern.lon = root.center.lon;
	root.child[1].northWestern.lat = root.center.lat;
	root.child[1].northWestern.lon = root.center.lon;
	root.child[1].southEastern.lat = root.northEastern.lat;
	root.child[1].southEastern.lon = root.southWestern.lon;
	root.child[1].southWestern.lat = root.center.lat;
	root.child[1].southWestern.lon = root.southWestern.lon;

	// Third child quad
	root.child[2].northEastern.lat = root.center.lat;
	root.child[2].northEastern.lon = root.northEastern.lon;
	root.child[2].northWestern.lat = root.southWestern.lat;
	root.child[2].northWestern.lon = root.northEastern.lon;
	root.child[2].southEastern.lat = root.center.lat;
	root.child[2].southEastern.lon = root.center.lon;
	root.child[2].southWestern.lat = root.southWestern.lat;
	root.child[2].southWestern.lon = root.center.lon;

	// Fourth child quad
	root.child[3].northEastern.lat = root.northEastern.lat;
	root.child[3].northEastern.lon = root.northEastern.lon;
	root.child[3].northWestern.lat = root.center.lat;
	root.child[3].northWestern.lon = root.northEastern.lon;
	root.child[3].southEastern.lat = root.northEastern.lat;
	root.child[3].southEastern.lon = root.center.lon;
	root.child[3].southWestern.lat = root.center.lat;
	root.child[3].southWestern.lon = root.center.lon;
}

// Function for finding center mass value
void findCenterMass(Quadrant &root) {
	float totalX = 0.00;
	float totalY = 0.00;
	for ( int i = 0; i < (int)root.cities.size(); i ++ ) {
		totalX = totalX + root.cities[i].point.lat;
		totalY = totalY + root.cities[i].point.lon;
	}
	root.center.lat = totalX / (float)root.cities.size();
	root.center.lon = totalY / (float)root.cities.size();
}

// Function for finding a length of diagonal haversine distance
void findDiagonal(Quadrant &root) {
	float diagonal = haversine(root.northEastern, root.southWestern);
	root.diagonal = diagonal;
}

// Function for initialization
void initialize(Quadrant &root) {
	// Highest and lowest
	for ( int i = 0; i < (int)root.cities.size(); i ++ ) {
		if ( i == 0 ) {
			root.southWestern.lat = root.cities[i].point.lat;
			root.southWestern.lon = root.cities[i].point.lon;
			root.northEastern.lat = root.cities[i].point.lat;
			root.northEastern.lon = root.cities[i].point.lon;
		} else {
			if ( root.southWestern.lat > root.cities[i].point.lat ) {
				root.southWestern.lat = root.cities[i].point.lat;
			}
			if ( root.southWestern.lon > root.cities[i].point.lon ) {
				root.southWestern.lon = root.cities[i].point.lon;
			}
			if ( root.northEastern.lat < root.cities[i].point.lat ) {
				root.northEastern.lat = root.cities[i].point.lat;
			}
			if ( root.northEastern.lon < root.cities[i].point.lon ) {
				root.northEastern.lon = root.cities[i].point.lon;
			}
		}
	}

	// Center mass of dataset
	findCenterMass(root);

	// Diagonal haversine distance
	findDiagonal(root);

	if ( root.diagonal <= EPSILON ) root.done = 1;
}

// Quadtree (Insert new child quadrant to parent quadrant)
void insertQuad(Quadrant &root) {
	if ( root.done == 0 ) {
		// Generate child quadrants first
		root.child.resize(4);

		// Divide
		for ( int i = 0; i < (int)root.cities.size(); i ++ ) {
			// First child quadrant
			if ( (root.cities[i].point.lat < root.center.lat) && 
			     (root.cities[i].point.lon < root.center.lon) ) {
				int numPoints = root.child[0].cities.size();
				root.child[0].cities.resize(numPoints+1);
				root.child[0].cities[numPoints] = root.cities[i];
			// Second child quadrant
			} else if ( (root.cities[i].point.lat >= root.center.lat) && 
				    (root.cities[i].point.lon < root.center.lon) ) {
				int numPoints = root.child[1].cities.size();
				root.child[1].cities.resize(numPoints+1);
				root.child[1].cities[numPoints] = root.cities[i];
			// Third child quadrant
			} else if ( (root.cities[i].point.lat < root.center.lat) && 
				    (root.cities[i].point.lon >= root.center.lon) ) {
				int numPoints = root.child[2].cities.size();
				root.child[2].cities.resize(numPoints+1);
				root.child[2].cities[numPoints] = root.cities[i];
			// Fourth child quadrant
			} else if ( (root.cities[i].point.lat >= root.center.lat) && 
				    (root.cities[i].point.lon >= root.center.lon) ) {
				int numPoints = root.child[3].cities.size();
				root.child[3].cities.resize(numPoints+1);
				root.child[3].cities[numPoints] = root.cities[i];
			}
		}
		
		// Highest and lowest values of each quadrant
		findEdgePointsQuadrant(root);

		// Center mass value and diagonal distance of each quadrant
		for ( int i = 0; i < (int)root.child.size(); ) {
			if ( root.child[i].cities.size() > 1 ) {
				findCenterMass(root.child[i]);
				findDiagonal(root.child[i]);
				if ( root.child[i].diagonal <= EPSILON ) {
					root.child[i].done = 1;
					numDataPoints = numDataPoints + (int)root.child[i].cities.size();
				}
				i++;
			} else if ( root.child[i].cities.size() == 1 ) {
				findCenterMass(root.child[i]);
				findDiagonal(root.child[i]);
				root.child[i].done = 1;
				numDataPoints = numDataPoints + (int)root.child[i].cities.size();
				i++;
			} else root.child.erase(root.child.begin() + i);
		}

		// Go further
		for ( int i = 0; i < (int)root.child.size(); i ++ ) {
			if ( root.child[i].done == 0 ) insertQuad(root.child[i]);
		}
	}
}

// Quadtree (Main)
void quadtree(Quadrant &root) {
	insertQuad(root);
}

// DBSCAN (Comparer between epsilon box and quadrant)
// Check the epsilon box is in a quadrant first
int comparePart1(std::vector<PointDBSCAN> &dataset, int index, Quadrant &root) {
	int numPoints = 0;
	if ( dataset[index].northEastern.lat >= root.southWestern.lat && 
	     dataset[index].northEastern.lat <= root.northEastern.lat &&
	     dataset[index].northEastern.lon >= root.southWestern.lon &&
	     dataset[index].northEastern.lon <= root.northEastern.lon ) numPoints++;
	if ( dataset[index].northWestern.lat >= root.southWestern.lat && 
	     dataset[index].northWestern.lat <= root.northEastern.lat &&
	     dataset[index].northWestern.lon >= root.southWestern.lon &&
	     dataset[index].northWestern.lon <= root.northEastern.lon ) numPoints++;
	if ( dataset[index].southEastern.lat >= root.southWestern.lat && 
	     dataset[index].southEastern.lat <= root.northEastern.lat &&
	     dataset[index].southEastern.lon >= root.southWestern.lon &&
	     dataset[index].southEastern.lon <= root.northEastern.lon ) numPoints++;
	if ( dataset[index].southWestern.lat >= root.southWestern.lat && 
	     dataset[index].southWestern.lat <= root.northEastern.lat &&
	     dataset[index].southWestern.lon >= root.southWestern.lon &&
	     dataset[index].southWestern.lon <= root.northEastern.lon ) numPoints++;
	return numPoints;
}
// If then, check how many edge points out of four are in epsilon box
int comparePart2(std::vector<PointDBSCAN> &dataset, int index, Quadrant &root) {
	int numPoints = 0;
	if ( root.northEastern.lat >= dataset[index].southWestern.lat && 
	     root.northEastern.lat <= dataset[index].northEastern.lat &&
	     root.northEastern.lon >= dataset[index].southWestern.lon &&
	     root.northEastern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root.northWestern.lat >= dataset[index].southWestern.lat && 
	     root.northWestern.lat <= dataset[index].northEastern.lat &&
	     root.northWestern.lon >= dataset[index].southWestern.lon &&
	     root.northWestern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root.southEastern.lat >= dataset[index].southWestern.lat && 
	     root.southEastern.lat <= dataset[index].northEastern.lat &&
	     root.southEastern.lon >= dataset[index].southWestern.lon &&
	     root.southEastern.lon <= dataset[index].northEastern.lon ) numPoints++;
	if ( root.southWestern.lat >= dataset[index].southWestern.lat && 
	     root.southWestern.lat <= dataset[index].northEastern.lat &&
	     root.southWestern.lon >= dataset[index].southWestern.lon &&
	     root.southWestern.lon <= dataset[index].northEastern.lon ) numPoints++;
	return numPoints;
}

// DBSCAN (Do haversine calculation based on candidate list)
void candidateListCalculator(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant &root) {
	for ( int i = 0; i < (int)root.cities.size(); i ++ ) {
		if ( dataset[index].point.lat != root.cities[i].point.lat && 
		     dataset[index].point.lon != root.cities[i].point.lon) {
			int datasetID = root.cities[i].datasetID;
			if ( dataset[datasetID].clusterID == UNCLASSIFIED || dataset[datasetID].clusterID == NOISE ) {
				if ( haversine(dataset[index].point, root.cities[i].point) <= EPSILON ) {
					if ( root.diagonal <= EPSILON ) {
						for ( int j = 0; j < (int)root.cities.size(); j ++ ) {
							if ( dataset[index].point.lat != root.cities[j].point.lat &&
							     dataset[index].point.lon != root.cities[j].point.lon ) {
								borders.push_back(root.cities[j].datasetID);
							}
						}
					} else {
						borders.push_back(root.cities[i].datasetID);
					}
				}
			}
		}
	}
}

void findQuadrantsPart2(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant &root) {
	int resultPart2 = comparePart2(dataset, index, root);
	if ( resultPart2 == 1 || resultPart2 == 2 || resultPart2 == 3 ) {
		if ( root.done == 0 ) {
			for ( int i = 0; i < (int)root.child.size(); i ++ ) {
				findQuadrantsPart2(dataset, index, borders, root.child[i]);
			}
		} else {
			candidateListCalculator(dataset, index, borders, root);
		}
	} else if ( resultPart2 == 4 ) {
		candidateListCalculator(dataset, index, borders, root);
	}
}

// DBSCAN (Do make candidate list)
void findQuadrantsPart1(std::vector<PointDBSCAN> &dataset, int index, std::vector<int> &borders, Quadrant &root) {
	int resultPart1 = comparePart1(dataset, index, root);
	if ( resultPart1 != 0 ) {
		if ( root.done == 1 ) {
			candidateListCalculator(dataset, index, borders, root);
		} else {
			int resultPart2 = comparePart2(dataset, index, root);
			if ( resultPart2 == 0 ) {
				for ( int i = 0; i < (int)root.child.size(); i ++ ) {
					findQuadrantsPart1(dataset, index, borders, root.child[i]);
				}
			} else findQuadrantsPart2(dataset, index, borders, root);
		}
	}
}

// DBSCAN (Border Point Finder of Core Point)
void borderFinderCore(std::vector<PointDBSCAN> &dataset, int corePoint, std::vector<int> &bordersCore, Quadrant &root) {
	bordersCore.resize(0);
	for ( int i = 0; i < (int)root.child.size(); i ++ ) {
		findQuadrantsPart1(dataset, corePoint, bordersCore, root.child[i]);
	}
}

// DBSCAN (Border Point Finder of Border Point)
void borderFinderBorder(std::vector<PointDBSCAN> &dataset, int borderPoint, std::vector<int> &bordersBorder, Quadrant &root) {
	bordersBorder.resize(0);
	for ( int i = 0; i < (int)root.child.size(); i ++ ) {
		findQuadrantsPart1(dataset, borderPoint, bordersBorder, root.child[i]);
	}
}

// DBSCAN (Cluster Expander)
int clusterExpander(std::vector<PointDBSCAN> &dataset, int index, int clusterID, Quadrant &root) {
	std::vector<int> bordersCore;
	std::vector<int> bordersBorder;
	borderFinderCore(dataset, index, bordersCore, root);

	if ( (int)bordersCore.size() < MINIMUM_POINTS ) {
		dataset[index].clusterID = NOISE;
		return FAILURE;
	} else {
		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			dataset[borderPoint].clusterID = clusterID;
		}

		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			int borderPoint = bordersCore[i];
			if ( (dataset[borderPoint].point.lat == dataset[index].point.lat) && 
			     (dataset[borderPoint].point.lon == dataset[index].point.lon) ) {
				continue;
			} else {
				borderFinderBorder(dataset, borderPoint, bordersBorder, root);

				if ( (int)bordersBorder.size() >= MINIMUM_POINTS ) {
					for ( int j = 0; j < (int)bordersBorder.size(); j ++ ) {
						int neighbourPoint = bordersBorder[j];
						//if ( dataset[neighbourPoint].clusterID == UNCLASSIFIED ||
						 //    dataset[neighbourPoint].clusterID == NOISE ) {
							if ( dataset[neighbourPoint].clusterID == UNCLASSIFIED ) {
								bordersCore.push_back(neighbourPoint);
							}
							dataset[neighbourPoint].clusterID = clusterID;
						//}
					}
				}
			}
		}
		return SUCCESS;
	}
}

// DBSCAN (Main)
int dbscan(std::vector<PointDBSCAN> &dataset, Quadrant &root) {
	int clusterID = 1;
	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		if ( dataset[i].clusterID == UNCLASSIFIED ) {
			if ( clusterExpander(dataset, i, clusterID, root) != FAILURE ) clusterID += 1;
		}
	}

	
	return clusterID-1;
}

// Function for printing results
void printResults(std::vector<PointDBSCAN> &dataset) {
	printf(" x       y       cluster_id\n"
	       "---------------------------\n");

	for ( int i = 0; i < (int)dataset.size(); i ++ ) {
		printf("%f, %f: %d\n", dataset[i].point.lat, dataset[i].point.lon, dataset[i].clusterID);
	}

	printf( "--------------------------\n" );
	printf( "Number of Points  : %d\n", (int)dataset.size() );
}

// Main
int main() {
	int numCities = 44691;

	std::vector<PointDBSCAN> dataset;
	Quadrant root;

	// Read point data
	char benchmark_filename[] = "../../../worldcities.bin";
	readBenchmarkDataDBSCAN(dataset, benchmark_filename, numCities);
	readBenchmarkDataQuadTree(root, benchmark_filename, numCities);

	// Initialize
	initialize(root);

	// Get four edge points of epsilon box of each data point
	printf( "Finding Four Edge Points of Epsilon Box of 44691 Cities Start!\n" );
	double processStartStep1 = timeCheckerCPU();
	findEdgePointsEpsilonBox(dataset);
	double processFinishStep1 = timeCheckerCPU();
	double processTimeStep1 = processFinishStep1 - processStartStep1;
	printf( "Finding Four Edge Points of Epsilon Box of Each Data Point Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// Quadtree
	printf( "Quadtree for 44691 Cities Start!\n" );
	double processStartStep2 = timeCheckerCPU();
	quadtree(root);
	double processFinishStep2 = timeCheckerCPU();
	double processTimeStep2 = processFinishStep2 - processStartStep2;
	printf( "Quadtree for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// DBSCAN
	printf( "DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStartStep3 = timeCheckerCPU();
	int maxClusterID = dbscan(dataset, root);
	double processFinishStep3 = timeCheckerCPU();
	double processTimeStep3 = processFinishStep3 - processStartStep3;
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// result of Quadtree-based DBSCAN algorithm
	printResults(dataset);
	printf( "Elapsed Time [Step1] (CPU): %.8f\n", processTimeStep1 );
	printf( "Elapsed Time [Step2] (CPU): %.8f\n", processTimeStep2 );
	printf( "Elapsed Time [Step3] (CPU): %.8f\n", processTimeStep3 );
	printf( "Max Cluster ID            : %d\n", maxClusterID );
	printf( "Num of Haversine          : %d\n", numHaversine );

	return 0;
}
