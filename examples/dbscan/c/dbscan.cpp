#include "dbscan.h"


Point m_points[NumPoints];
uint32_t m_pointSize = 0;
uint32_t m_minPoints = 0;
float m_epsilon = 0.00;


void readBenchmarkData() {
	// load point cloud
	FILE *stream;
	stream = fopen ("benchmark_hepta.dat","ra");

	uint32_t num_points, cluster, i = 0;
	fscanf(stream, "%u\n", &num_points);

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

inline float calculateDistance(const Point& pointCore, const Point& pointTarget ) {
	float dx = pow((pointCore.x - pointTarget.x), 2);
	float dy = pow((pointCore.y - pointTarget.y), 2);
	float dz = pow((pointCore.z - pointTarget.z), 2);
	return dx + dy+ dz;
}

std::vector<int> calculateCluster(Point point) {
	std::vector<int> clusterIndex;
	
	for( int i = 0; i < NumPoints; i ++ ) {
		if ( calculateDistance(point, m_points[i]) <= m_epsilon ) clusterIndex.push_back(i);
	}
	return clusterIndex;
}

int expandCluster(Point point, int clusterID) {    
	std::vector<int> clusterSeeds = calculateCluster(point);
	
	if ( clusterSeeds.size() < m_minPoints ) {
		point.clusterID = NOISE;
		return FAILURE;
	} else {
		int index = 0;
	       	int indexCorePoint = 0;
		
		for( int i = 0; i < (int)clusterSeeds.size(); i ++ ) {
			int pointBorder = clusterSeeds[i];
			m_points[pointBorder].clusterID = clusterID;
		
			if (m_points[pointBorder].x == point.x && 
			    m_points[pointBorder].y == point.y && 
			    m_points[pointBorder].z == point.z ) {
				indexCorePoint = index;
			}
			++index;
		}
		clusterSeeds.erase(clusterSeeds.begin()+indexCorePoint);
		
		for( int i = 0; i < (int)clusterSeeds.size(); i ++ ) {
			int pointBorder = clusterSeeds[i];
			std::vector<int> clusterNeighors = calculateCluster(m_points[pointBorder]);
			
			if ( clusterNeighors.size() >= m_minPoints ) {
				for ( int j = 0; j < (int)clusterNeighors.size(); j ++ ) {
					int pointNeighor = clusterNeighors[j];
					if ( m_points[pointNeighor].clusterID == UNCLASSIFIED || 
					     m_points[pointNeighor].clusterID == NOISE ) {
						if ( m_points[pointNeighor].clusterID == UNCLASSIFIED ) {
							clusterSeeds.push_back(pointNeighor);
						}
						m_points[pointNeighor].clusterID = clusterID;
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
