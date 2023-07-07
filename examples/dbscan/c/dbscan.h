#ifndef DBSCAN_H
#define DBSCAN_H


#include <vector>
#include <cmath>
#include <stdint.h>


#define CORE_POINT 1
#define BORDER_POINT 2

#define SUCCESS 0
#define UNCLASSIFIED -1
#define NOISE -2
#define FAILURE -3


typedef struct Point {
	float x, y, z;  // 3D data
	int clusterID;  // clustered ID
}Point;

typedef struct DBSCAN {
	std::vector<Point> m_points;
	uint32_t m_pointSize;
	uint32_t m_minPoints;
	float m_epsilon;
}DBSCAN;

void initialize(DBSCAN *ds, uint32_t minPts, float eps, std::vector<Point> points);
inline float calculateDistance(const Point& pointCore, const Point& pointTarget);
std::vector<int> calculateCluster(DBSCAN *ds, Point point);
int expandCluster(DBSCAN *ds, Point point, int clusterID);
void run(DBSCAN *ds);
int getTotalPointSize(DBSCAN *ds);
int getMinClusterSize(DBSCAN *ds);
int getEpsilon(DBSCAN *ds);

#endif // DBSCAN_H
