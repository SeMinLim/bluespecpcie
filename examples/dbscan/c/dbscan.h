#ifndef DBSCAN_H
#define DBSCAN_H


#include <vector>
#include <cmath>


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

class DBSCAN {
public:    
	std::vector<Point> m_points;
	unsigned int m_pointSize;
	unsigned int m_minPoints;
	float m_epsilon;

	DBSCAN(unsigned int minPts, float eps, std::vector<Point> points) {
		m_minPoints = minPts;
		m_epsilon = eps;
		m_points = points;
		m_pointSize = points.size();
	}
	~DBSCAN(){}

	int getTotalPointSize() {return m_pointSize;}
	int getMinimumClusterSize() {return m_minPoints;}
	int getEpsilonSize() {return m_epsilon;}

	int run();
	std::vector<int> calculateCluster(Point point);
	int expandCluster(Point point, int clusterID);
	inline double calculateDistance(const Point& pointCore, const Point& pointTarget);	
};

#endif // DBSCAN_H
