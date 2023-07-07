#include "dbscan.h"


void initialize(DBSCAN *ds, uint32_t minPts, float eps, std::vector<Point> points) {
	ds->m_minPoints = minPts;
	ds->m_epsilon = eps;
	ds->m_points = points;
	ds->m_pointSize = points.size();
}

inline float calculateDistance(const Point& pointCore, const Point& pointTarget ) {
	float dx = pow((pointCore.x - pointTarget.x), 2);
	float dy = pow((pointCore.y - pointTarget.y), 2);
	float dz = pow((pointCore.z - pointTarget.z), 2);
	return dx + dy+ dz;
}

std::vector<int> calculateCluster(DBSCAN *ds, Point point) {
	int index = 0;
	
	std::vector<Point>::iterator iter;
	std::vector<int> clusterIndex;
	
	for( iter = ds->m_points.begin(); iter != ds->m_points.end(); ++iter ) {
		if ( calculateDistance(point, *iter) <= ds->m_epsilon ) clusterIndex.push_back(index);
		index++;
	}
	return clusterIndex;
}

int expandCluster(DBSCAN *ds, Point point, int clusterID) {    
	std::vector<int> clusterSeeds = calculateCluster(ds, point);
	
	if ( clusterSeeds.size() < ds->m_minPoints ) {
		point.clusterID = NOISE;
		return FAILURE;
	} else {
		int index = 0;
	       	int indexCorePoint = 0;
		std::vector<int>::iterator iterSeeds;
		
		for( iterSeeds = clusterSeeds.begin(); iterSeeds != clusterSeeds.end(); ++iterSeeds ) {
			ds->m_points.at(*iterSeeds).clusterID = clusterID;
		
			if (ds->m_points.at(*iterSeeds).x == point.x && 
			    ds->m_points.at(*iterSeeds).y == point.y && 
			    ds->m_points.at(*iterSeeds).z == point.z ) {
				indexCorePoint = index;
			}
			++index;
		}
		clusterSeeds.erase(clusterSeeds.begin()+indexCorePoint);
		
		for( std::vector<int>::size_type i = 0, n = clusterSeeds.size(); i < n; ++i ) {
			std::vector<int> clusterNeighors = calculateCluster(ds, ds->m_points.at(clusterSeeds[i]));
			
			if ( clusterNeighors.size() >= ds->m_minPoints ) {
				std::vector<int>::iterator iterNeighors;
				for ( iterNeighors = clusterNeighors.begin(); iterNeighors != clusterNeighors.end(); ++iterNeighors ) {
					if ( ds->m_points.at(*iterNeighors).clusterID == UNCLASSIFIED || 
					     ds->m_points.at(*iterNeighors).clusterID == NOISE ) {
						if ( ds->m_points.at(*iterNeighors).clusterID == UNCLASSIFIED ) {
							clusterSeeds.push_back(*iterNeighors);
							n = clusterSeeds.size();
						}
						ds->m_points.at(*iterNeighors).clusterID = clusterID;
					}
				}
			}
		}
		return SUCCESS;
	}
}

void run(DBSCAN *ds) {
	int clusterID = 1;
	std::vector<Point>::iterator iter;
	
	for( iter = ds->m_points.begin(); iter != ds->m_points.end(); ++iter ) {
		if ( iter->clusterID == UNCLASSIFIED ) {
			if ( expandCluster(ds, *iter, clusterID) != FAILURE ) clusterID += 1;
		}
	}
}
