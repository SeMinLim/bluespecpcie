#include <sys/resource.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>


// ZFP Conditions
// Bit Budget 1  -> 17  = 8x2+1  [3]
// Bit Budget 2  -> 21  = 8x2+5  [3]
// Bit Budget 3  -> 25  = 8x3+1  [4]
// Bit Budget 4  -> 29  = 8x3+5  [4]
// Bit Budget 5  -> 33  = 8x4+1  [5]
// Bit Budget 6  -> 37  = 8x4+5  [5]
// Bit Budget 7  -> 41  = 8x5+1  [6]
// Bit Budget 8  -> 45  = 8x5+5  [6]
// Bit Budget 9  -> 49  = 8x6+1  [7]
// Bit Budget 10 -> 53  = 8x6+5  [7]
// Bit Budget 11 -> 57  = 8x7+1  [8]
// Bit Budget 12 -> 61  = 8x7+5  [8]
// Bit Budget 13 -> 65  = 8x8+1  [9]
// Bit Budget 14 -> 69  = 8x8+5  [9]
// Bit Budget 15 -> 73  = 8x9+1  [10]
// Bit Budget 16 -> 77  = 8x9+5  [10]
// Bit Budget 17 -> 81  = 8x10+1 [11]
// Bit Budget 18 -> 85  = 8x10+5 [11]
// Bit Budget 19 -> 89  = 8x11+1 [12]
// Bit Budget 20 -> 93  = 8x11+5 [12]
// Bit Budget 21 -> 97  = 8x12+1 [13]
// Bit Budget 22 -> 101 = 8x12+5 [13]
// Bit Budget 23 -> 105 = 8x13+1 [14]
// Bit Budget 24 -> 109 = 8x13+5 [14]
// Bit Budget 25 -> 113 = 8x14+1 [15]
// Bit Budget 26 -> 117 = 8x14+5 [15]
// Bit Budget 27 -> 121 = 8x15+1 [16]
// Bit Budget 28 -> 125 = 8x15+5 [16]
// Bit Budget 29 -> 129 = 8x16+1 [17]
// Bit Budget 30 -> 133 = 8x16+5 [17]
// Bit Budget 31 -> 137 = 8x17+1 [18]


// ZFP
#define BIT_BUDGET 11
// Exponent of single is 8 bit signed integer (-126 to +127)
#define EXP_MAX ((1<<(8-1))-1)
// Exponent of the minimum granularity value expressible via single
#define ZFP_MIN_EXP -149
#define ZFP_MAX_PREC 32
#define EBITS (8+1)


typedef struct Point {
	float lat;
	float lon;
}Point;


class BitBuffer {
public:
	BitBuffer(int bytes);
	~BitBuffer();
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
BitBuffer::~BitBuffer() {
	free(this->buffer);
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
void decompress_1d(uint8_t comp[8], BitBuffer* compressed, float* output, int bit_budget) {
	uint32_t e;
	for ( int i = 0; i < 8; i ++ ) {
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

// Compressor
void compressor(Point pointCore, Point pointTarget, uint8_t *compPoint) {
	BitBuffer* output = new BitBuffer(4*sizeof(float));
	float originPoint[4];

	originPoint[0] = pointCore.lat;
	originPoint[1] = pointCore.lon;
	originPoint[2] = pointTarget.lat;
	originPoint[3] = pointTarget.lon;
	
	compress_1d(originPoint, output, BIT_BUDGET);
	
	for ( int c = 0; c < 8; c ++ ) {
		compPoint[c] = output->buffer[c];
	}

	delete output;
}

// Decompressor
void decompressor(uint8_t compPoint[8], Point &pointCore, Point &pointTarget) {
	float decompPoint[4];
	BitBuffer* compressed = new BitBuffer(4*sizeof(float));
	
	decompress_1d(compPoint, compressed, decompPoint, BIT_BUDGET);
	delete compressed;

	pointCore.lat = decompPoint[0];
	pointCore.lon = decompPoint[1];
	pointTarget.lat = decompPoint[2];
	pointTarget.lon = decompPoint[3];
}

// Elapsed time checker
static inline double timeCheckerCPU(void) {
        struct rusage ru;
        getrusage(RUSAGE_SELF, &ru);
        return (double)ru.ru_utime.tv_sec + (double)ru.ru_utime.tv_usec / 1000000;
}

// Function for reading benchmark file
void readBenchmarkData(std::vector<Point> &cities, char* filename, int length) {
	FILE* f_data = fopen(filename, "rb");
	if (f_data == NULL ) {
		printf("File not found: %s\n", filename);
		exit(1);
	}

	for ( int i = 0; i < length; i ++ ) {
		int numPoints = cities.size();
		cities.resize(numPoints+1);
		fread(&cities[i].lat, sizeof(float), 1, f_data);
		fread(&cities[i].lon, sizeof(float), 1, f_data);
	}

	fclose(f_data);
}


// Main
int main(int argc, char **argv)
{
	int numCities = 700968*1;

	std::vector<Point> cities(1);

	// Read point data
	char benchmark_filename[] = "../worldcities_augmented.bin";
	readBenchmarkData(cities, benchmark_filename, numCities);	

	// Compression
	uint8_t compPoint[8] = {0,};
	Point pointCore = cities[0];
	Point pointTarget = cities[1];
	compressor(pointCore, pointTarget, &compPoint[0]);

	// Decompression
	decompressor(&compPoint[0], pointCore, pointTarget);
	printf( "Bit Budget : %d\n", BIT_BUDGET );
	printf( "Original => [core]: %f, %f [Target]: %f, %f\n", cities[0].lat, cities[0].lon, cities[1].lat, cities[1].lon );
	printf( "Compress => [core]: %f, %f [Target]: %f, %f\n", pointCore.lat, pointCore.lon, pointTarget.lat, pointTarget.lon );
	
	return 0;
}
