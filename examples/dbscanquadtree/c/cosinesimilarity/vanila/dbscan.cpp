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
#define EPSILON 0.9


int numCosineSimilarity = 0;


typedef struct Point {
	float lat, lon; // latitude & longitude
	int clusterID; 	// Cluster ID
}Point;

typedef struct Cluster {
	std::vector<Point> cities;
	Point lowest;
	Point highest;
	Point center;
	float diagonal;
	int done;
}Cluster;

typedef struct Index {
	int quadrantIdx;
	int cityIdx;
}Index;


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

// Cosine Similarity
float cosineSimilarity(const Point pointCore, const Point pointTarget) {
	numCosineSimilarity++;
	float xy = pointCore.lat*pointTarget.lat + pointCore.lon*pointTarget.lon;
	
	float mag_x = sqrt(pow(pointCore.lat, 2.00) + pow(pointCore.lon, 2.00));
	float mag_y = sqrt(pow(pointTarget.lat, 2.00) + pow(pointTarget.lon, 2.00));
	float mag_xy = mag_x * mag_y;

	return xy / mag_xy;
}

// Function for finding lowest and highest points
void findHighestLowest(std::vector<Cluster> &quadrants, int firstChildQuad, int parentQuad) {
	// First child quad
	quadrants[firstChildQuad].highest.lat = quadrants[parentQuad].center.lat;
	quadrants[firstChildQuad].highest.lon = quadrants[parentQuad].center.lon;
	quadrants[firstChildQuad].lowest.lat = quadrants[parentQuad].lowest.lat;
	quadrants[firstChildQuad].lowest.lon = quadrants[parentQuad].lowest.lon;

	// Second child quad
	quadrants[firstChildQuad+1].highest.lat = quadrants[parentQuad].highest.lat;
	quadrants[firstChildQuad+1].highest.lon = quadrants[parentQuad].center.lon;
	quadrants[firstChildQuad+1].lowest.lat = quadrants[parentQuad].center.lat;
	quadrants[firstChildQuad+1].lowest.lon = quadrants[parentQuad].lowest.lon;

	// Third child quad
	quadrants[firstChildQuad+2].highest.lat = quadrants[parentQuad].center.lat;
	quadrants[firstChildQuad+2].highest.lon = quadrants[parentQuad].highest.lon;
	quadrants[firstChildQuad+2].lowest.lat = quadrants[parentQuad].lowest.lat;
	quadrants[firstChildQuad+2].lowest.lon = quadrants[parentQuad].center.lon;

	// Fourth child quad
	quadrants[firstChildQuad+3].highest.lat = quadrants[parentQuad].highest.lat;
	quadrants[firstChildQuad+3].highest.lon = quadrants[parentQuad].highest.lon;
	quadrants[firstChildQuad+3].lowest.lat = quadrants[parentQuad].center.lat;
	quadrants[firstChildQuad+3].lowest.lon = quadrants[parentQuad].center.lon;
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

// Function for finding a length of diagonal haversine distance
void findDiagonal(std::vector<Cluster> &quadrants, int quadrantIdx) {
	float diagonal = cosineSimilarity(quadrants[quadrantIdx].highest, quadrants[quadrantIdx].lowest);
	quadrants[quadrantIdx].diagonal = diagonal;
}

// Function for initialization
void initialize(std::vector<Cluster> &quadrants) {
	// Highest and lowest
	for ( int i = 0; i < (int)quadrants[0].cities.size(); i ++ ) {
		if ( i == 0 ) {
			quadrants[0].lowest.lat = quadrants[0].cities[i].lat;
			quadrants[0].lowest.lon = quadrants[0].cities[i].lon;
			quadrants[0].highest.lat = quadrants[0].cities[i].lat;
			quadrants[0].highest.lon = quadrants[0].cities[i].lon;
		} else {
			if ( quadrants[0].lowest.lat > quadrants[0].cities[i].lat ) {
				quadrants[0].lowest.lat = quadrants[0].cities[i].lat;
			}
			if ( quadrants[0].lowest.lon > quadrants[0].cities[i].lon ) {
				quadrants[0].lowest.lon = quadrants[0].cities[i].lon;
			}
			if ( quadrants[0].highest.lat < quadrants[0].cities[i].lat ) {
				quadrants[0].highest.lat = quadrants[0].cities[i].lat;
			}
			if ( quadrants[0].highest.lon < quadrants[0].cities[i].lon ) {
				quadrants[0].highest.lon = quadrants[0].cities[i].lon;
			}
		}
	}

	// Center mass of dataset
	findCenterMass(quadrants, 0);

	// Diagonal haversine distance
	findDiagonal(quadrants, 0);

	if ( quadrants[0].diagonal >= EPSILON ) quadrants[0].done = 1;
}

// Quadtree
void quadtree(std::vector<Cluster> &quadrants) {
	int flag = false;
	while(1) {
		int initNumQuad = (int)quadrants.size();
		int firstChildQuad = initNumQuad;

		// Decide parent quadrant
		int parentQuad = 0;
		for ( int i = 0; i < initNumQuad; i ++ ) {
			if ( quadrants[i].done == 0 ) {
				parentQuad = i;
				quadrants.resize(initNumQuad+4);
				break;
			} else {
				if ( i == initNumQuad - 1 ) flag = true;
			}
		}

		// Decide to terminate quadtree or not
		if ( flag == true ) break;

		// Quadtree
		for ( int i = 0; i < (int)quadrants[parentQuad].cities.size(); i ++ ) {
			// First child quadrant
			if ( (quadrants[parentQuad].cities[i].lat < quadrants[parentQuad].center.lat) && 
			     (quadrants[parentQuad].cities[i].lon < quadrants[parentQuad].center.lon) ) {
				int numPoints = quadrants[firstChildQuad].cities.size();
				quadrants[firstChildQuad].cities.resize(numPoints+1);
				quadrants[firstChildQuad].cities[numPoints] = quadrants[parentQuad].cities[i];
			// Second child quadrant
			} else if ( (quadrants[parentQuad].cities[i].lat >= quadrants[parentQuad].center.lat) && 
				    (quadrants[parentQuad].cities[i].lon < quadrants[parentQuad].center.lon) ) {
				int numPoints = quadrants[firstChildQuad+1].cities.size();
				quadrants[firstChildQuad+1].cities.resize(numPoints+1);
				quadrants[firstChildQuad+1].cities[numPoints] = quadrants[parentQuad].cities[i];
			// Third child quadrant
			} else if ( (quadrants[parentQuad].cities[i].lat < quadrants[parentQuad].center.lat) && 
				    (quadrants[parentQuad].cities[i].lon >= quadrants[parentQuad].center.lon) ) {
				int numPoints = quadrants[firstChildQuad+2].cities.size();
				quadrants[firstChildQuad+2].cities.resize(numPoints+1);
				quadrants[firstChildQuad+2].cities[numPoints] = quadrants[parentQuad].cities[i];
			// Fourth child quadrant
			} else if ( (quadrants[parentQuad].cities[i].lat >= quadrants[parentQuad].center.lat) && 
				    (quadrants[parentQuad].cities[i].lon >= quadrants[parentQuad].center.lon) ) {
				int numPoints = quadrants[firstChildQuad+3].cities.size();
				quadrants[firstChildQuad+3].cities.resize(numPoints+1);
				quadrants[firstChildQuad+3].cities[numPoints] = quadrants[parentQuad].cities[i];
			}
		}

		// Highest and lowest values of each quadrant
		findHighestLowest(quadrants, firstChildQuad, parentQuad);

		// Center mass value and diagonal haversine distance of each quadrant
		for ( int i = firstChildQuad; i < (int)quadrants.size(); ) {
			if ( quadrants[i].cities.size() > 1 ) {
				findCenterMass(quadrants, i);
				findDiagonal(quadrants, i);
				if ( quadrants[i].diagonal >= EPSILON ) quadrants[i].done = 1;
				i++;
			} else if ( quadrants[i].cities.size() == 1 ) {
				findCenterMass(quadrants, i);
				quadrants[i].done = 1;
				i++;
			} else quadrants.erase(quadrants.begin() + i);
		}

		// Delete parent quadrant or not
		if ( initNumQuad != (int)quadrants.size() ) {
			quadrants.erase(quadrants.begin() + parentQuad);
		} else quadrants[parentQuad].done = 1;
	}
}

// DBSCAN (Border Point Finder of Core Point)
void borderFinderCore(std::vector<Cluster> &quadrants, Index point, std::vector<Index> &bordersCore) {
	bordersCore.resize(0);

	Index border;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		if ( quadrants[point.quadrantIdx].cities[point.cityIdx].lat != quadrants[i].cities[0].lat &&
		     quadrants[point.quadrantIdx].cities[point.cityIdx].lon != quadrants[i].cities[0].lon ) {
			if ( quadrants[i].cities[0].clusterID == UNCLASSIFIED || quadrants[i].cities[0].clusterID == NOISE ) {
				if ( cosineSimilarity(quadrants[point.quadrantIdx].cities[point.cityIdx], quadrants[i].cities[0]) >= EPSILON ) {
					if ( quadrants[i].diagonal >= EPSILON ) {
						for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
							border.quadrantIdx = i;
							border.cityIdx = j;
							bordersCore.push_back(border);
						}
					} else {
						border.quadrantIdx = i;
						border.cityIdx = 0;
						bordersCore.push_back(border);
					}
				}
			}
		}
	}
}

// DBSCAN (Border Point Finder of Border Point)
void borderFinderBorder(std::vector<Cluster> &quadrants, Index point, std::vector<Index> &bordersBorder) {
	bordersBorder.resize(0);

	Index border;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		if ( quadrants[point.quadrantIdx].cities[point.cityIdx].lat != quadrants[i].cities[0].lat &&
		     quadrants[point.quadrantIdx].cities[point.cityIdx].lon != quadrants[i].cities[0].lon ) {
			if ( quadrants[i].cities[0].clusterID == UNCLASSIFIED || quadrants[i].cities[0].clusterID == NOISE ) {
				if ( cosineSimilarity(quadrants[point.quadrantIdx].cities[point.cityIdx], quadrants[i].cities[0]) >= EPSILON ) {
					if ( quadrants[i].diagonal >= EPSILON ) {
						for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
							border.quadrantIdx = i;
							border.cityIdx = j;
							bordersBorder.push_back(border);
						}
					} else {
						border.quadrantIdx = i;
						border.cityIdx = 0;
						bordersBorder.push_back(border);
					}
				}
			}
		}
	}
}


// DBSCAN (Cluster Expander)
int clusterExpander(std::vector<Cluster> &quadrants, Index point, int clusterID) {
	std::vector<Index> bordersCore;
	std::vector<Index> bordersBorder;
	borderFinderCore(quadrants, point, bordersCore);

	if ( (int)bordersCore.size() < MINIMUM_POINTS ) {
		quadrants[point.quadrantIdx].cities[point.cityIdx].clusterID = NOISE;
		return FAILURE;
	} else {
		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			Index borderPoint = bordersCore[i];
			quadrants[borderPoint.quadrantIdx].cities[borderPoint.cityIdx].clusterID = clusterID;
		}

		for ( int i = 0; i < (int)bordersCore.size(); i ++ ) {
			Index borderPoint = bordersCore[i];
			if ( (quadrants[borderPoint.quadrantIdx].cities[borderPoint.cityIdx].lat == 
			      quadrants[point.quadrantIdx].cities[point.cityIdx].lat) && 
			     (quadrants[borderPoint.quadrantIdx].cities[borderPoint.cityIdx].lon == 
			      quadrants[point.quadrantIdx].cities[point.cityIdx].lon) ) {
				continue;
			} else {
				borderFinderBorder(quadrants, borderPoint, bordersBorder);

				if ( (int)bordersBorder.size() >= MINIMUM_POINTS ) {
					for ( int j = 0; j < (int)bordersBorder.size(); j ++ ) {
						Index neighbourPoint = bordersBorder[j];
						if ( quadrants[neighbourPoint.quadrantIdx].cities[neighbourPoint.cityIdx].clusterID == UNCLASSIFIED ) {
							bordersCore.push_back(neighbourPoint);
						}
						quadrants[neighbourPoint.quadrantIdx].cities[neighbourPoint.cityIdx].clusterID = clusterID;
					}
				}
			}
		}
		return SUCCESS;
	}
}

// DBSCAN (Main)
int dbscan(std::vector<Cluster> &quadrants) {
	int clusterID = 1;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
			if ( quadrants[i].cities[j].clusterID == UNCLASSIFIED ) {
				Index point;
				point.quadrantIdx = i;
				point.cityIdx = j;
				if ( clusterExpander(quadrants, point, clusterID) != FAILURE ) clusterID += 1;
			}
		}
	}
	
	return clusterID-1;
}

// Function for printing results
void printResults(std::vector<Cluster> &quadrants) {
	printf(" x       y       cluster_id\n"
	       "---------------------------\n");

	int numDataPoints = 0;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
			printf("%f, %f: %d\n", quadrants[i].cities[j].lat, quadrants[i].cities[j].lon, 
					       quadrants[i].cities[j].clusterID);
		}
		numDataPoints = numDataPoints + (int)quadrants[i].cities.size();
	}

	printf( "--------------------------\n" );
	printf( "Number of Points  : %d\n", numDataPoints );
}

// Main
int main() {
	int numCities = 44691;

	std::vector<Cluster> quadrants(1);

	// Read point data
	char benchmark_filename[] = "../../../worldcities.bin";
	readBenchmarkData(quadrants, benchmark_filename, numCities);

	// Initialize
	initialize(quadrants);
	
	// Quadtree
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStart = timeCheckerCPU();
	quadtree(quadrants);

	// DBSCAN
	int maxClusterID = dbscan(quadrants);
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// result of Quadtree-based DBSCAN algorithm
	printResults(quadrants);
	printf( "Elapsed Time (CPU): %.8f\n", processTime );
	printf( "Max Cluster ID    : %d\n", maxClusterID );
	printf( "Num of Cos Sim    : %d\n", numCosineSimilarity );

	return 0;
}
