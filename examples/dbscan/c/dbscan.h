#ifndef DBSCAN_H
#define DBSCAN_H


#include <math.h>
#include <stdint.h>
#include <stdio.h>


#define CORE_POINT 1
#define BORDER_POINT 2

#define SUCCESS 0
#define UNCLASSIFIED -1
#define NOISE -2
#define FAILURE -3

#define NumPoints 212

typedef struct Point {
	float x, y, z;  // 3D data
	int clusterID;  // clustered ID
}Point;

void initialize(uint32_t minPts, float eps);
float calculateDistance(const Point pointCore, const Point pointTarget);
void calculateClusterSeeds(Point point);
void calculateClusterNeighbours(Point point);
int expandCluster(Point point, int clusterID);
void run();
void readBenchmarkData();
void printResults();

#endif // DBSCAN_H
