#include "dbscan.h"


#define MINIMUM_POINTS 4     // minimum number of cluster
#define EPSILON (0.75*0.75)  // distance for clustering, metre^2


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
