#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "bdbmpcie.h"
#include "dmasplitter.h"


// Parameter setting
#define SEQNUM 26
#define SEQLENGTH 111
#define MOTIFLENGTH 11


// Elapsed time checker
double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

// Function for reading benchmark file
void readfromfile( char* data, char* filename, uint32_t length ) {
	FILE* f_data = fopen(filename, "rb");
	if (f_data == NULL ) {
		printf("File not found: %s\n", filename);
		exit(1);
	}

	fread(data, sizeof(char), length, f_data);

	fclose(f_data);
}

// Function for representing sequences as 2-bit
void convertCharTo2Bits( uint8_t* sequences2Bits, char* sequences, uint32_t num, uint32_t length ) {
	uint32_t idx = 0;
	uint8_t cnt = 0;
	uint8_t unit = 0;
	for ( uint32_t i = 0; i < num; i ++ ) {
		for ( uint32_t j = 0; j < length; j ++ ) {
			if ( sequences[length*i+j] == 'A' ) unit = (0 << 2*cnt) | unit;
			else if ( sequences[length*i+j] == 'C' ) unit = (1 << 2*cnt) | unit;
			else if ( sequences[length*i+j] == 'G' ) unit = (2 << 2*cnt) | unit;
			else if ( sequences[length*i+j] == 'T' ) unit = (3 << 2*cnt) | unit;
			
			if ( cnt == 3 ) {
				sequences2Bits[idx++] = unit;
				cnt = 0;
				unit = 0;
			} else cnt ++;
		}
		
		sequences2Bits[idx++] = unit;
		cnt = 0;
		unit = 0;
		
		for ( uint32_t k = 0; k < 8; k ++ ) {
			sequences2Bits[idx++] = unit;
		}
	}
}

// Function for representing 2-bit encoded sequences to original form
void convert2BitsToChar( char* sequences, uint8_t* sequences2Bits, uint32_t length, uint32_t remainder ) {
	uint32_t idx = 0;
	for ( uint32_t i = 0; i < length; i ++ ) {
		uint8_t fourChar = sequences2Bits[i];
		if ( i == length - 1 ) {
			for ( uint32_t k = 0; k < remainder; k ++ ) {
				uint8_t oneChar = fourChar << 2*(4-k-1);
				oneChar = oneChar >> 6;
				if ( oneChar == 0 ) sequences[idx++] = 'A';
				else if ( oneChar == 1 ) sequences[idx++] = 'C';
				else if ( oneChar == 2 ) sequences[idx++] = 'G';
				else if ( oneChar == 3 ) sequences[idx++] = 'T';
			}
		} else {
			for ( uint32_t k = 0; k < 4; k ++ ) {
				uint8_t oneChar = fourChar << 2*(4-k-1);
				oneChar = oneChar >> 6;
				if ( oneChar == 0 ) sequences[idx++] = 'A';
				else if ( oneChar == 1 ) sequences[idx++] = 'C';
				else if ( oneChar == 2 ) sequences[idx++] = 'G';
				else if ( oneChar == 3 ) sequences[idx++] = 'T';
			}
		}
	}
}


// Main
int main(int argc, char** argv) {
	BdbmPcie* pcie = BdbmPcie::getInstance();
	uint8_t* dmabuf = (uint8_t*)pcie->dmaBuffer();

	unsigned int d = pcie->readWord(0);
	printf( "Magic: %x\n", d );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage1: Read a file that contains the DNA sequences
	//-------------------------------------------------------------------------------
	uint32_t sizeQuery = SEQNUM*SEQLENGTH;
	char* filenameQuery = "cpp/dataset/mm9Gata4MotifCollection.bin";
	char* sequences = (char*)malloc(sizeof(char)*sizeQuery);
	readfromfile(&sequences[0], filenameQuery, sizeQuery);
	//-------------------------------------------------------------------------------
	// Stage2: Convert char to 2bits (encoding) 
	// For fitting DRAM interface (512bits),
	// 2 sequences (444bits) and zero padding (68bits) are needed
	//-------------------------------------------------------------------------------
	uint32_t sequences2BitsSize = 64 * 13;
	uint8_t* sequences2Bits = (uint8_t*)malloc(sizeof(uint8_t)*sequences2BitsSize);
	convertCharTo2Bits(&sequences2Bits[0], &sequences[0], 13, 222);
	//-------------------------------------------------------------------------------
	// Stage3: Send the sequences to FPGA through DMA
	// 832 x 1B(8bits) = 52 x 16B(128bits)
	//-------------------------------------------------------------------------------
	for ( uint32_t i = 0; i < sequences2BitsSize; i ++ ) {
		dmabuf[i] = sequences2Bits[i];
	}
	pcie->userWriteWord(0, 52);
	//-------------------------------------------------------------------------------
	// Stage4: Make a set of answer
	//-------------------------------------------------------------------------------
	uint32_t sizeAnswer = SEQNUM*MOTIFLENGTH;
	char* answers = (char*)malloc(sizeof(char)*sizeAnswer);
	for ( uint32_t i = 0; i < SEQNUM; i ++ ) {
		for ( uint32_t j = 0; j < MOTIFLENGTH; j ++ ) {
			answers[MOTIFLENGTH*i+j] = sequences[SEQLENGTH*i+j];
		}
	}
	//-------------------------------------------------------------------------------
	// Stage5: Receive the data from HW through DMA
	// 26 x 11 x 2bits = 572bits
	// 572bits = 4 x 128bits (512bits) + 60bits
	// 572bits = 71 x 8bits (568bits) + 4bits
	// 5 128bits are stored in DMA buffer
	//-------------------------------------------------------------------------------
	uint32_t lsb11Chars2BitsSize = 72;
	uint32_t lsb11Chars2BitsRmdr = 2;
	uint8_t* lsb11Chars2Bits = (uint8_t*)malloc(sizeof(uint8_t)*lsb11Chars2BitsSize);
	char* lsb11Chars = (char*)malloc(sizeof(char)*sizeAnswer);
	// Send we want to read 5 128bits buffers
	pcie->userWriteWord(4, 5);
	// For wait
	for ( int z = 0; z < 1024*1024*32; z ++ ) {
		pcie->userWriteWord(8, 0);
	}
	for ( uint32_t i = 0; i < lsb11Chars2BitsSize; i ++ ) {
		lsb11Chars2Bits[i] = dmabuf[i];
	}
	//-------------------------------------------------------------------------------
	// Stage6: Decode received 2-bit encoded 11 LSB characters
	//-------------------------------------------------------------------------------
	convert2BitsToChar(&lsb11Chars[0], &lsb11Chars2Bits[0], lsb11Chars2BitsSize, lsb11Chars2BitsRmdr);
	//-------------------------------------------------------------------------------
	// Stage7: Print 11 LSB characters and compare with answers
	//-------------------------------------------------------------------------------
	char lsb11Char[MOTIFLENGTH+1];
	char answer[MOTIFLENGTH+1];
	for ( int i = 0; i < SEQNUM; i ++ ) {
		for ( int j = 0; j < MOTIFLENGTH; j ++ ) {
			lsb11Char[j] = lsb11Chars[MOTIFLENGTH*i + j];
			answer[j] = answers[MOTIFLENGTH*i + j];
		}
		answer[MOTIFLENGTH] = '\0';
		lsb11Char[MOTIFLENGTH] = '\0';
		
		if ( strncmp(lsb11Char, answer, 11) == 0 ) {
			printf( "%s            Match               %s\n", lsb11Char, answer );
		} else {
			printf( "%s            Unmatch             %s\n", lsb11Char, answer );
		}
		
	}

	free(sequences);
	free(sequences2Bits);
	free(lsb11Chars);
	free(lsb11Chars2Bits);
	free(answers);

	return 0;
}
