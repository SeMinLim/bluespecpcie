#ifndef DBSCAN_H
#define DBSCAN_H


#include <vector>
#include <cmath>
#include <stdint.h>
#include <stdio.h>
#include <iostream>


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
inline float calculateDistance(const Point& pointCore, const Point& pointTarget);
std::vector<int> calculateCluster(Point point);
int expandCluster(Point point, int clusterID);
void run();
void readBenchmarkData();
void printResults();

#endif // DBSCAN_H
