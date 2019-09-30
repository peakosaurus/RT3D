#pragma once
#include <vector>
#include "windows.h"



// Note that these are set to reflect the size of a 4-bit, 32x32 TIM (576 Bytes), and other data sizes vary
struct CLUT_BLOCK
{
	int clutSize;
	short frameBufferX;
	short frameBufferY;
	short dataSizeX;
	short dataSizeY;
	short clutData[16];
};

struct PIXEL_DATA_BLOCK
{
	int pixelBlockSize;
	short frameBufferX;
	short frameBufferY;
	short dataSizeX;
	short dataSizeY;
	short pixelData[(32*32)/4];
};

struct TIM_FILE 
{
	int colourMode;
	int clutMode;
	CLUT_BLOCK clut;
	PIXEL_DATA_BLOCK pixel;
};


struct SHARED_CLUT 
{
	short clutData[16];
	int	channels[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	vector<int> sharedRefs;
};


struct VERTEXINFO
{
	INT16	sHeight;
	UINT16	sRGBLighting;
}; // 4 bytes				


struct POLYSTRUCT
{
	UINT8 u0, v0;
	UINT16 clut;
	UINT8 u1, v1;
	UINT16 tpage;
	UINT8 u2, v2;
	UINT16 sInfo;
	UINT8 u3, v3;
	UINT8 cRot, cTileRef;
	// 00000000 00000111	-> 0-7  - Rotation of Texture
};// 16 bytes

struct SEGMENT
{
	// Segment Origin
	INT16		sOriginZ, sOriginX;
	INT16		sOriginY;

	// Info used for grouping segments
	UINT16 		sSegmentInfo; // 8 bytes

	// Dynamic RGB and static intensity valuses
	VERTEXINFO	strVertexInfo[25]; // 100 bytes

	// Tile texture information
	POLYSTRUCT 	strTilePolyStruct[16]; // 256 bytes

	// Link to objects occupying tile
	UINT32 obpWorldObjectPtr; // 4 bytes 
}; 	// 368 Bytes



int FatalError(const char*);
int UnmanglePTM(char*, TIM_FILE* &);
int Unmangle(unsigned char *, unsigned char *);
int DrawTIM(TIM_FILE&, int, int);
Color GetCLUTcolour(CLUT_BLOCK& clut, int index);
Color GetOriginalCLUTcolour(SHARED_CLUT& clut, int index);
int CalculateUniqueCLUTs(TIM_FILE* pTims, int timCount, vector<SHARED_CLUT> &vCLUTs);
int UnmanglePMM(char* filename, SEGMENT* &pSegments);
int DrawSegments2Buffer(SEGMENT* pSegments );
int SaveChannelPNGs(string &folderPath, SEGMENT* pSegmentData, TIM_FILE* pTIMData);
int SaveDiffusePNG(string &folderPath, SEGMENT* pSegmentData, TIM_FILE* pTIMData);
int SaveChannelPNGs(string & folderPath, SEGMENT * pSegmentData, TIM_FILE * pTIMData);
int LoadCLUTChannelData(string &folderPath);
int SaveCLUTChannelData(string &folderPath);
int LoadPNGThumbnails(string &folderPath);
int LoadChannelThumbnails(string &folderPath);