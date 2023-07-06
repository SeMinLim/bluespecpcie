#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)


float haversine(float lat1, float lon1, float lat2, float lon2)
{
	// Distance between latitudes and longitudes
	float dlat = (lat2-lat1)*TO_RADIAN;
	float dlon = (lon2-lon1)*TO_RADIAN;

	// Convert to radians
	lat1 = lat1*TO_RADIAN;
	lat2 = lat2*TO_RADIAN;

	// Apply formula
	float f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(lat1) * cos(lat2);
	return asin(sqrt(f)) * 2 * EARTH_RADIUS;
}


int main()
{
	float d = haversine(37.547889, 126.997128, 35.158874, 129.043846);
	
	printf("dist: %.4f km (%.4f mi.)\n", d, (d/1.609344));

	return 0;
}
