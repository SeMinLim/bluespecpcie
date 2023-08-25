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
#define EPSILON 2000.00

// Haversine
#define EARTH_RADIUS 6371
#define TO_RADIAN (3.1415926536 / 180)

// ZFP
#define BIT_BUDGET 5 
// Exponent of single is 8 bit signed integer (-126 to +127)
#define EXP_MAX ((1<<(8-1))-1)
// Exponent of the minimum granularity value expressible via single
#define ZFP_MIN_EXP -149
#define ZFP_MAX_PREC 32
#define EBITS (8+1)


typedef struct Point {
	float lat, lon;
	int clusterID;
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


class BitBuffer {
public:
	BitBuffer(int bytes);
	void EncodeBit(uint32_t src);
	void EncodeBits(uint32_t src, int bits);
	int BitCount() { return curbits; };

	void DecodeBit(uint32_t* dst);
	void DecodeBits(uint32_t* dst, int bits);

	uint8_t* buffer;
	int bufferbytes;
	int curbits;
	int decoff;
};
BitBuffer::BitBuffer(int bytes) {
	this->buffer = (uint8_t*)malloc(bytes);
	for ( int i = 0; i < bytes; i++ ) this->buffer[i] = 0;

	this->bufferbytes = bytes;
	this->curbits = 0;
	this->decoff = 0;
}
void BitBuffer::EncodeBit(uint32_t src) {
	int byteoff = (curbits/8);
	int bitoff = curbits%8;
	buffer[byteoff] |= ((src&1)<<bitoff);

	curbits++;
}
void BitBuffer::EncodeBits(uint32_t src, int bits) {
	for ( int i = 0; i < bits; i++ ) {
		EncodeBit(src);
		src >>= 1;
	}
}	
void BitBuffer::DecodeBit(uint32_t* dst) {
	int byteoff = (decoff/8);
	int bitoff = decoff%8;
	uint8_t buf = buffer[byteoff];
	*dst = (buf>>bitoff) & 1;
	decoff++;
}
void BitBuffer::DecodeBits(uint32_t* dst, int bits) {
	*dst = 0;
	for ( int i = 0; i < bits; i++ ) {
		uint32_t o = 0;
		DecodeBit(&o);
		*dst |= (o<<i);
	}
}


// Convert negabinary uint to int
static int32_t uint2int_uint32(uint32_t x) {
	return (int32_t)((x ^ 0xaaaaaaaa) - 0xaaaaaaaa);
}

// Convert int to negabinary uint
static uint32_t int2uint_int32(int32_t x) {
	return ((uint32_t)x + 0xaaaaaaaa) ^ 0xaaaaaaaa;
}

// Forward lifting
static void fwd_lift_int32(int32_t* p, uint s) {
	int32_t x, y, z, w;
	x = *p; p += s;
	y = *p; p += s;
	z = *p; p += s;
	w = *p; p += s;
	
	x += w; x >>= 1; w -= x;
	z += y; z >>= 1; y -= z;
	x += z; x >>= 1; z -= x;
	w += y; w >>= 1; y -= w;
	w += y >> 1; y -= w >> 1;
	
	p -= s; *p = w;
	p -= s; *p = z;
	p -= s; *p = y;
	p -= s; *p = x;
}

// Inverse lifting
static void inv_lift_int32(int32_t* p, uint32_t s) {
	int32_t x, y, z, w;
	x = *p; p += s;
	y = *p; p += s;
	z = *p; p += s;
	w = *p; p += s;
	
	y += w >> 1; w -= y >> 1;
	y += w; w <<= 1; w -= y;
	z += x; x <<= 1; x -= z;
	y += z; z <<= 1; z -= y;
	w += x; x <<= 1; x -= w;
	
	p -= s; *p = w;
	p -= s; *p = z;
	p -= s; *p = y;
	p -= s; *p = x;
}

// Compressor
void compress_1d(float origin[4], BitBuffer* decompressed, int bit_budget) {
	int exp_max = -EXP_MAX;
	for ( int i = 0; i < 4; i++ ) {
		if ( origin[i] != 0 ) {
			int exp = 0;
			std::frexp(origin[i], &exp);
			if ( exp > exp_max ) exp_max = exp;
		}
	}
	int dimension = 1;
	int precision_max = std::min(ZFP_MAX_PREC, std::max(0, exp_max - ZFP_MIN_EXP + (2*(dimension+1))));
	if ( precision_max != 0 ) {
		int e = exp_max + EXP_MAX;
		int32_t idata[4];
		for ( int i = 0; i < 4; i++ ) {
			idata[i] = (int32_t)(origin[i]*(pow(2, 32-2 - exp_max)));
		}
		
		// perform lifting
		fwd_lift_int32(idata, 1);
		
		// convert to negabinary
		uint32_t udata[4];
		for ( int i = 0; i < 4; i++ ) { 
			udata[i] = int2uint_int32(idata[i]);
		}

		int total_bits = EBITS;
		decompressed->EncodeBits(e, EBITS);

		for ( int i = 0; i < 4; i++ ) {
			uint32_t u = udata[i];
			if ( (u>>28) == 0 ) {
				decompressed->EncodeBit(0);
				decompressed->EncodeBits(u>>(32-bit_budget-4), bit_budget);
				total_bits += bit_budget + 1;
			} else {
				decompressed->EncodeBit(1);
				decompressed->EncodeBits(u>>(32-bit_budget), bit_budget);
				total_bits += bit_budget + 1;
			}
		}
	}
}

// Decompressor
void decompress_1d(uint8_t comp[4], BitBuffer* compressed, float* output, int bit_budget) {
	uint32_t e;
	for ( int i = 0; i < 4; i ++ ) {
		compressed->EncodeBits(comp[i], 8);
	}
	compressed->EncodeBits(0, 1);
	compressed->DecodeBits(&e, EBITS);
	int exp_max = ((int)e) - EXP_MAX;
	int dimension = 1;
	int precision_max = std::min(ZFP_MAX_PREC, std::max(0, exp_max - ZFP_MIN_EXP + (2*(dimension+1))));

	uint32_t udata[4] = {0,};
	for ( int i = 0; i < 4; i++ ) {
		uint32_t flag;
		uint32_t bits;
		compressed->DecodeBit(&flag);
		compressed->DecodeBits(&bits, bit_budget);

		if ( flag == 0 ) {
			udata[i] = (bits<<(32-bit_budget-4));
		} else {
			udata[i] = (bits<<(32-bit_budget));
		}
	}

	int32_t idata[4];

	for ( int i = 0; i < 4; i++ ) {
		idata[i] = uint2int_uint32(udata[i]);
	}
	
	inv_lift_int32(idata, 1);

	for ( int i = 0; i < 4; i++ ) {
		float q = pow(2,exp_max-(32-2));
		output[i] = ((float)idata[i]*q);
	}
}

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

// Haversine
float haversine_comp(uint8_t compPoint[4]) {
	// Decompress first
	float decompPoint[4];
	BitBuffer* compressed = new BitBuffer(4*sizeof(float));
	decompress_1d(compPoint, compressed, decompPoint, BIT_BUDGET);
	delete compressed;

	Point pointCore;
	Point pointTarget;
	
	pointCore.lat = decompPoint[0];
	pointCore.lon = decompPoint[1];
	pointTarget.lat = decompPoint[2];
	pointTarget.lon = decompPoint[3];

	// Distance between latitudes and longitudes
	float dlat = (pointTarget.lat-pointCore.lat)*TO_RADIAN;
	float dlon = (pointTarget.lon-pointCore.lon)*TO_RADIAN;

	// Convert to radians
	float rad_lat_core = pointCore.lat*TO_RADIAN;
	float rad_lat_target = pointTarget.lat*TO_RADIAN;

	// Apply formula
	float f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(rad_lat_core) * cos(rad_lat_target);
	return asin(sqrt(f)) * 2 * EARTH_RADIUS;
}
float haversine(const Point pointCore, const Point pointTarget ) {
	// Distance between latitudes and longitudes
	float dlat = (pointTarget.lat-pointCore.lat)*TO_RADIAN;
	float dlon = (pointTarget.lon-pointCore.lon)*TO_RADIAN;

	// Convert to radians
	float rad_lat_core = pointCore.lat*TO_RADIAN;
	float rad_lat_target = pointTarget.lat*TO_RADIAN;

	// Apply formula
	float f = pow(sin(dlat/2),2) + pow(sin(dlon/2),2) * cos(rad_lat_core) * cos(rad_lat_target);
	return asin(sqrt(f)) * 2 * EARTH_RADIUS;
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
	float diagonal = haversine(quadrants[quadrantIdx].highest, quadrants[quadrantIdx].lowest);
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

	if ( quadrants[0].diagonal <= EPSILON ) quadrants[0].done = 1;
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
				if ( quadrants[i].diagonal <= EPSILON ) quadrants[i].done = 1;
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
				BitBuffer* output = new BitBuffer(4*sizeof(float));
				uint8_t compPoint[4];
				float originPoint[4];
				originPoint[0] = quadrants[point.quadrantIdx].cities[point.cityIdx].lat;
				originPoint[1] = quadrants[point.quadrantIdx].cities[point.cityIdx].lon;
				originPoint[2] = quadrants[i].cities[0].lat;
				originPoint[3] = quadrants[i].cities[0].lon;
				compress_1d(originPoint, output, BIT_BUDGET);
				for ( int c = 0; c < 4; c ++ ) {
					compPoint[c] = output->buffer[c];
				}
				if ( haversine_comp(compPoint) <= EPSILON ) {
					if ( quadrants[i].diagonal <= EPSILON ) {
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
				BitBuffer* output = new BitBuffer(4*sizeof(float));
				uint8_t compPoint[4];
				float originPoint[4];
				originPoint[0] = quadrants[point.quadrantIdx].cities[point.cityIdx].lat;
				originPoint[1] = quadrants[point.quadrantIdx].cities[point.cityIdx].lon;
				originPoint[2] = quadrants[i].cities[0].lat;
				originPoint[3] = quadrants[i].cities[0].lon;
				compress_1d(originPoint, output, BIT_BUDGET);
				for ( int c = 0; c < 4; c ++ ) {
					compPoint[c] = output->buffer[c];
				}
				if ( haversine_comp(compPoint) <= EPSILON ) {
					if ( quadrants[i].diagonal <= EPSILON ) {
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
void dbscan(std::vector<Cluster> &quadrants) {
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
	printf( "Max Cluster ID: %d\n", clusterID - 1 );
}

// Function for printing results
void printResults(std::vector<Cluster> &quadrants) {
	printf(" x     y     cluster_id\n"
	       "-----------------------\n");

	int numDataPoints = 0;
	for ( int i = 0; i < (int)quadrants.size(); i ++ ) {
		for ( int j = 0; j < (int)quadrants[i].cities.size(); j ++ ) {
			printf("%f, %f: %d\n", quadrants[i].cities[j].lat, quadrants[i].cities[j].lon, 
					       quadrants[i].cities[j].clusterID);
		}
		numDataPoints = numDataPoints + (int)quadrants[i].cities.size();
	}

	printf( "Number of Points: %d\n", numDataPoints );
}

// Main
int main() {
	int numCities = 44691;

	std::vector<Cluster> quadrants(1);

	// Read point data
	char benchmark_filename[] = "../../worldcities.bin";
	readBenchmarkData(quadrants, benchmark_filename, numCities);

	// Initialize
	initialize(quadrants);
	
	// Quadtree
	quadtree(quadrants);

	// DBSCAN
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Start!\n" );
	double processStart = timeCheckerCPU();
	dbscan(quadrants);
	double processFinish = timeCheckerCPU();
	double processTime = processFinish - processStart;
	printf( "Quadtree-based DBSCAN Clustering for 44691 Cities Done!\n" );
	printf( "\n" );
	fflush( stdout );

	// result of Quadtree-based DBSCAN algorithm
	printResults(quadrants);
	printf( "Elapsed Time (CPU): %.6f\n", processTime );
	
	return 0;
}
