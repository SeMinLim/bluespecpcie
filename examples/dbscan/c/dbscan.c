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

#define MINIMUM_POINTS 4     // minimum number of cluster
#define EPSILON (0.75*0.75)  // distance for clustering, metre^2i


typedef struct Point {
	float x, y, z;  // 3D data
	int clusterID;  // clustered ID
}Point;


Point m_points[NumPoints];
int clusterSeeds[NumPoints];
int clusterNeighbours[NumPoints];

uint32_t m_pointSize = 0;
uint32_t m_minPoints = 0;
float m_epsilon = 0.00;

uint16_t clusterSeedsSize = 0;
uint16_t clusterNeighboursSize = 0;


void readBenchmarkData() {
	FILE *stream;
	stream = fopen ("benchmark.dat","ra");

	uint32_t cluster, i = 0;

	while (i < NumPoints) {
		fscanf(stream, "%f,%f,%f,%d\n", &(m_points[i].x), &(m_points[i].y), &(m_points[i].z), &cluster);
		m_points[i].clusterID = UNCLASSIFIED;
		i++;
	}

	fclose(stream);
}

void initialize(uint32_t minPts, float eps) {
	m_minPoints = minPts;
	m_epsilon = eps;
	m_pointSize = NumPoints;
}

float calculateDistance(const Point pointCore, const Point pointTarget ) {
	float dx = pow((pointCore.x - pointTarget.x), 2);
	float dy = pow((pointCore.y - pointTarget.y), 2);
	float dz = pow((pointCore.z - pointTarget.z), 2);
	return dx + dy + dz;
}

void calculateClusterSeeds(Point point) {
	clusterSeedsSize = 0;
	
	for( int i = 0; i < NumPoints; i ++ ) {
		if ( calculateDistance(point, m_points[i]) <= m_epsilon ) {
			clusterSeeds[clusterSeedsSize++] = i;
		}
	}
}

void calculateClusterNeighbours(Point point) {
	clusterNeighboursSize = 0;

	for( int i = 0; i < NumPoints; i ++ ) {
		if ( calculateDistance(point, m_points[i]) <= m_epsilon ) {
			clusterNeighbours[clusterNeighboursSize++] = i;
		}
	}
}

int expandCluster(Point point, int clusterID) {    
	calculateClusterSeeds(point);
	
	if ( clusterSeedsSize < m_minPoints ) {
		point.clusterID = NOISE;
		return FAILURE;
	} else {
		for( int i = 0; i < clusterSeedsSize; i ++ ) {
			int pointBorder = clusterSeeds[i];
			m_points[pointBorder].clusterID = clusterID;
		}
			
		for( int i = 0; i < clusterSeedsSize; i ++ ) {
			int pointBorder = clusterSeeds[i];
			if ( m_points[pointBorder].x == point.x &&
			     m_points[pointBorder].y == point.y &&
			     m_points[pointBorder].z == point.z ) {
				continue;
			} else {
				calculateClusterNeighbours(m_points[pointBorder]);
			
				if ( clusterNeighboursSize >= m_minPoints ) {
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

void run() {
	int clusterID = 1;
	for( int i = 0; i < NumPoints; i ++ ) {
		if ( m_points[i].clusterID == UNCLASSIFIED ) {
			if ( expandCluster(m_points[i], clusterID) != FAILURE ) clusterID += 1;
		}
	}
}

void printResults() {
	int i = 0;

	printf("Number of points: %u\n"
	       " x     y     z     cluster_id\n"
	       "-----------------------------\n", NumPoints);

	while (i < NumPoints) {
		printf("%5.2lf %5.2lf %5.2lf: %d\n", m_points[i].x, m_points[i].y, m_points[i].z, m_points[i].clusterID);
		++i;
	}
}

int main() {
	// read point data
	readBenchmarkData();
	
	// Initialize	
	initialize(MINIMUM_POINTS, EPSILON);

	// main loop
	run();
	
	// result of DBSCAN algorithm
	printResults();    

	return 0;
}
