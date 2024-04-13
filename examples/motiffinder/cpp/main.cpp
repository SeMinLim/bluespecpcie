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
#define SEQNUM 56000
#define SEQLENGTH 1000
#define MOTIFLENGTH 16


// Elapsed time checker
double timespec_diff_sec( timespec start, timespec end ) {
	double t = end.tv_sec - start.tv_sec;
	t += ((double)(end.tv_nsec - start.tv_nsec)/1000000000L);
	return t;
}

// Function for representing sequences as 2-bit
void encoderSeq( uint8_t* sequencesEncoded, char* sequences ) {
	uint32_t idx = 0;
	uint8_t cnt = 0;
	uint8_t unit = 0;
	for ( uint32_t i = 0; i < SEQNUM; i ++ ) {
		for ( uint32_t j = 0; j < SEQLENGTH; j ++ ) {
			if ( sequences[SEQLENGTH*i + j] == 'A' ) unit = (0 << 2*cnt) | unit;
			else if ( sequences[SEQLENGTH*i + j] == 'C' ) unit = (1 << 2*cnt) | unit;
			else if ( sequences[SEQLENGTH*i + j] == 'G' ) unit = (2 << 2*cnt) | unit;
			else if ( sequences[SEQLENGTH*i + j] == 'T' ) unit = (3 << 2*cnt) | unit;
		
			if ( cnt == 3 ) {
				sequencesEncoded[idx++] = unit;
				cnt = 0;
				unit = 0;
			} else cnt ++;
		}

		unit = 0;
		
		// 48bits zero padding
		for ( uint32_t j = 0; j < 6; j ++ ) {
			sequencesEncoded[idx++] = unit;
		}
	}
}

// Function for representing motifs as 2-bit
void encoderMtf( uint8_t* motifsEncoded, char* motifs ) {
	uint32_t idx = 0;
	uint8_t cnt = 0;
	uint8_t unit = 0;
	for ( uint32_t i = 0; i < SEQNUM; i ++ ) {
		for ( uint32_t j = 0; j < MOTIFLENGTH; j ++ ) {
			if ( motifs[MOTIFLENGTH*i + j] == 'A' ) unit = (0 << 2*cnt) | unit;
			else if ( motifs[MOTIFLENGTH*i + j] == 'C' ) unit = (1 << 2*cnt) | unit;
			else if ( motifs[MOTIFLENGTH*i + j] == 'G' ) unit = (2 << 2*cnt) | unit;
			else if ( motifs[MOTIFLENGTH*i + j] == 'T' ) unit = (3 << 2*cnt) | unit;
		
			if ( cnt == 3 ) {
				motifsEncoded[idx++] = unit;
				cnt = 0;
				unit = 0;
			} else cnt++;
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
	uint32_t seqIdx = 0;
	char seqLine[1024];
	char *sequences = (char*)malloc(sizeof(char)*SEQNUM*SEQLENGTH);
	char *filename_sequences = "TFAP2A.fasta";
	FILE *f_data_sequences = fopen(filename_sequences, "r");
	if ( f_data_sequences == NULL ) {
		printf( "File not found: %s\n", filename_sequences );
		exit(1);
	}
	while ( fgets(seqLine, sizeof(seqLine), f_data_sequences) != NULL ) {
		if ( seqLine[0] != '>' ) {
			for ( int i = 0; i < SEQLENGTH; i ++ ) {
				sequences[seqIdx++] = seqLine[i];
			}
		}
	}
	fclose(f_data_sequences);
	printf( "[SW]: Reading the sequence file finished!\n" );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage2: Convert char to 2bits (encoding) 
	// 1 sequence = 2000bits
	// 512 x 4 => 2000bits for a sequence and 48bits for zero padding
	//-------------------------------------------------------------------------------
	uint32_t size = SEQNUM*(2048/8);
	uint8_t *sequencesEncoded = (uint8_t*)malloc(sizeof(uint8_t)*size);
	encoderSeq(&sequencesEncoded[0], &sequences[0]);
	printf( "[SW]: Encoding the sequences finished!\n" );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage4: Designate motif of each sequence randomly
	//-------------------------------------------------------------------------------
	char *motifs = (char*)malloc(sizeof(char)*SEQNUM*MOTIFLENGTH);
	for ( int i = 0; i < SEQNUM; i ++ ) {
		int sp = rand() % (SEQLENGTH - MOTIFLENGTH + 1);
		for ( int j = 0; j < MOTIFLENGTH; j ++ ) {
			motifs[MOTIFLENGTH*i + j] = sequences[SEQLENGTH*i + sp + j];
		}
	}
	printf( "[SW]: Designating the motifs randomly finished!\n" );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage5: Do 2-bit encoding for the motifs
	// To fit 128bits interface of DMA buffer,
	// 1 128bits buffer can consist of 4 2bits encoded motifs
	// We need 14000 128bits buffers
	//-------------------------------------------------------------------------------
	uint8_t *motifsEncoded = (uint8_t*)malloc(sizeof(uint8_t)*14000*(128/8));
	encoderMtf(&motifsEncoded[0], &motifs[0]);
	printf( "[SW]: Encoding the motifs finished!\n" );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage3: Send the sequences to FPGA through DMA
	// SEQNUM x (2048/8) x 1B(8bits) = SEQNUM x (2048/128) x 16B(128bits)
	//-------------------------------------------------------------------------------
	uint32_t idx = 0;
	while ( idx < size ) {
		// 8KB
		for ( uint32_t i = 0; i < 8*1024; i ++ ) {
			dmabuf[i] = sequencesEncoded[idx++];

			if ( idx == size ) break;
		}
		if ( (idx == size) && (size % (512*16) != 0) ) {
			uint32_t left = idx % (512*16);
			if ( left % 16 == 0 ) pcie->userWriteWord(0, (left/16));
			else pcie->userWriteWord(0, (left/16)+1);
		} else pcie->userWriteWord(0, 512);
	}
	printf( "[SW]: Sending the encoded sequences to DMA buffer finished!\n" );
	fflush( stdout );
	//-------------------------------------------------------------------------------
	// Stage6: Send the motifs to FPGA through DMA
	// 14000 x 16B(128bits)
	//-------------------------------------------------------------------------------
	idx = 0;
	size = 14000*(128/8);
	while ( idx < size ) {
		// 8KB
		for ( uint32_t i = 0; i < 8*1024; i ++ ) {
			dmabuf[i] = motifsEncoded[idx++];

			if ( idx == size ) break;
		}

		if ( (idx == size) && (size % (512*16) != 0) ) {
			uint32_t left = idx % (512*16);
			if ( left % 16 == 0 ) pcie->userWriteWord(0, (left/16));
			else pcie->userWriteWord(0, (left/16)+1);
		} else pcie->userWriteWord(0, 512);
	}
	printf( "[SW]: Sending the encoded motifs finished!\n" );
	fflush( stdout );

	for ( uint32_t i = 0; i < 2*1024*1024; i ++ ) {
		pcie->userWriteWord(8, 0);
	}
	if ( pcie->userReadWord(0) == 1 ) {
		printf( "[SW]: Reading DMA buffer on HW-side successfully finished!\n" );
		fflush( stdout );
	}
	
	for ( uint32_t i = 0; i < 32*1024*1024; i ++ ) {
		pcie->userWriteWord(8, 0);
	}

	free(sequences);
	free(sequencesEncoded);
	free(motifs);
	free(motifsEncoded);

	return 0;
}
