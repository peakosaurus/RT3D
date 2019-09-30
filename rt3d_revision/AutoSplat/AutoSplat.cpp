using namespace std;

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string> 
#include <vector>
#include "drawing.h"
#include "AutoSplat.h"


// These macros define the positions of everything on the screen
#define _TIMXPOS(t) (1 + (33 * (int)((t) % 8)))
#define _TIMYPOS(t) (24 + (33 * (int)floor((t) / 8)))

#define _CLUTXPOS(c) (268 + (33 * (int)((c) % 8)))
#define _CLUTYPOS(c) (57 + (33 * (int)floor((c) / 8)))

#define _COLXPOS(c) (268 + (33 * (int)((c) % 8)))
#define _COLYPOS(c) (g_clutBase + 33 + (33 * (int)floor((c) / 8)))

#define _CHXPOS(c) (268 + (33 * (int)((c) % 3)))
#define _CHYPOS(c) (g_clutBase + 132 + (36 * (int)floor((c) / 3)))

#define _MAPXPOS(p) (32 * (int)(p % 64))
#define _MAPYPOS(p) (32 * (int)floor(p / 64))

#define _TEXTUREXPOS(t) (1024*((int)floor(t / 256)%2)) + (64 * (int)(t % 16))
#define _TEXTUREYPOS(t) (1024*(int)floor(t / 512)) + (64 * (int)floor((t % 256) / 16))

#define _CHTEXTXPOS(c) (268 + (65 * (int)((c) % 3)))
#define _CHTEXTYPOS(c) (g_clutBase + 270 + (68 * (int)floor((c) / 3)))

#define SPLAT_CHANNELS 9

#define BUFX	534
#define	BUFY	24

#define MESSX	534
#define	MESSY	1056

uint32_t* g_pMapBuffer = 0;
uint32_t* g_pThumbBuffer = 0;
vector<SHARED_CLUT> g_vCLUTs;
vector<string> g_vTextures;
Color g_chanCol[9] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFC800, 0xFF00D8D8, 0xFFFF00FF, 0xFFFFFFFF, 0xFF808080, 0xFF7F4000 };
int g_clutBase = 0;

// Once upon a time...
int main()
{  
	// Not officially a console app, but we can grab the arguments this way in a Win32 app
	int argc = __argc;
	char** argv = __argv;
	string fileAndPath(argv[1]);
	TIM_FILE* pTIMData = 0;
	SEGMENT* pSegmentData = 0;
	g_pMapBuffer = new uint32_t[2048 * 2048];
	g_pThumbBuffer = new uint32_t[2048 * 2048];
	int xscroll = 0;
	int yscroll = 0;
	bool bRestoredData = false;

	// **************************************
	//  Load and initialise
	// **************************************

	if (argc < 2)
		return FatalError("Error: No file provided");

	if (fileAndPath.find(".PTM") == string::npos)
		return FatalError("Error: AutoSplat can only process files with the PTM file extension.");

	int timCount = UnmanglePTM( argv[1], pTIMData );

	if( timCount <= 0 )
		return FatalError("Error: Error loading TIM data from .PTM file");

	int uniqueCLUTs = CalculateUniqueCLUTs(pTIMData, timCount, g_vCLUTs);

	int segmentCount = UnmanglePMM(argv[1], pSegmentData);

	if( segmentCount < 256 )
		return FatalError("Error: AutoSplat requires a matching file with the PMM file extension in the same directory as the PTM.");

	// **************************************
	//  Intial screen draw 
	// **************************************
	DrawString(6, 4, string(argv[1]) + ": " + to_string(timCount) + " TIMs", MakeColor(255, 255, 255));

	for (int t = 0; t < timCount; t++)
		DrawTIM(pTIMData[t], _TIMXPOS(t), _TIMYPOS(t));

	int timBase = _TIMYPOS(timCount - 1) + 34;

	DrawString(273, 36, to_string(uniqueCLUTs) + " unique CLUTs", MakeColor(255, 255, 255));

	for (int u = 0; u < uniqueCLUTs; u++)
		DrawTIM(pTIMData[g_vCLUTs[u].sharedRefs[0]], _CLUTXPOS(u), _CLUTYPOS(u));

	g_clutBase = _CLUTYPOS(uniqueCLUTs - 1) + 34;

	DrawSegments2Buffer(pSegmentData);
	BlitPixels(g_pMapBuffer, BUFX, BUFY, 0, 0, 1024, 1024, 2048);

	// **************************************
	//  Prepare directory structure
	// **************************************

	// Create a new folder for the project files based on the root of the PTM file
	string folderPath = fileAndPath.replace(fileAndPath.find(string(".PTM")), 4, string(""));

	if (CreateDirectoryA(folderPath.c_str(), NULL))
		SaveDiffusePNG(folderPath, pSegmentData, pTIMData); // First time file has been opened
	else if (GetLastError() == ERROR_ALREADY_EXISTS)
		bRestoredData = LoadCLUTChannelData(folderPath); // Subsequent times
	else
		return FatalError("Error: Unable to create directory for map data.");

	LoadChannelThumbnails(folderPath);

	// **************************************
	//  Display keyboard controls
	// **************************************

	DrawString( 303, 730, "KEYBOARD CONTROLS", MakeColor(255, 255, 0));
	DrawString( 303, 736, "__________________", MakeColor(255, 255, 0));
	DrawString( 274, 770, "'M': Map Mode (default)", MakeColor(255, 255, 0));
	DrawString(274, 800, "'T': Texture Mode", MakeColor(255, 255, 0));
	DrawString(274, 830, "Arrow Keys: Scroll", MakeColor(255, 255, 0));
	DrawString(274, 860, "'S': Save Splat Layers", MakeColor(255, 255, 0));
	
	// **************************************
	// **************************************
	//  MAIN UPDATE LOOP
	// **************************************
	// **************************************

	int selCLUT = 0, selChan = 0, selCol = 0, selTexture = 0, lightTexture = 0;
	int prevSelCLUT = 1, prevSelChan = 1, prevSelCol = 1, prevSelTexture = 0, prevLightTexture = 0;
	bool bExit = false;
	int frame = 0;
	enum mode { MODE_MAP = 0, MODE_TEXTURES };
	mode theMode = MODE_MAP;

	while ( !bExit )
	{
		bool bUpdateCLUTs = false , bUpdateColours = false, bUpdateChannels = false, bUpdateSegments = false, bUpdateTIMs = false, bUpdateTextures = false;

		if( bRestoredData )
			{ bUpdateCLUTs = true, bUpdateColours = true, bUpdateChannels = true, bUpdateSegments = true; bUpdateTIMs = true; bRestoredData = false; }

		// ***********************************************************************
		//  Process mouse clicks against selectable cluts, channels and colours
		// ***********************************************************************
		if (LeftMousePressed())
		{
			for (int u = 0; u < uniqueCLUTs; u++)
			{
				if (MouseX() > _CLUTXPOS(u) && MouseX() < _CLUTXPOS(u) + 32 && MouseY() > _CLUTYPOS(u) && MouseY() < _CLUTYPOS(u) + 32)
					selCLUT = u;
			}

			for (int c = 0; c < SPLAT_CHANNELS; c++)
			{
				if (MouseX() > _CHXPOS(c) && MouseX() < _CHXPOS(c) + 32 && 	MouseY() > _CHYPOS(c) && MouseY() < _CHYPOS(c) + 32)
					selChan = c;
			}
		}

		if (theMode == MODE_TEXTURES)
		{
			lightTexture = -1;

			for (int t = 0; t < 256; t++)
			{
				if (MouseX() > BUFX + _TEXTUREXPOS(t) && MouseX() < BUFX + _TEXTUREXPOS(t) + 64 && MouseY() > BUFY + _TEXTUREYPOS(t) && MouseY() < BUFY + _TEXTUREYPOS(t) + 64)
				{
					int page = ((xscroll / 1024) * 256) + ((yscroll / 1024) * 512);

					if (LeftMousePressed())
						selTexture = t + page;
					else
						lightTexture = t + page;
				}
			}
		}

		for (int c = 0; c < 16; c++)
		{
			if (MouseX() > _COLXPOS(c) && MouseX() < _COLXPOS(c) + 32 && MouseY() > _COLYPOS(c) && MouseY() < _COLYPOS(c) + 32)
			{
				if (LeftMousePressed())
					{ selCol = c; g_vCLUTs[selCLUT].channels[c] = selChan;	}
				
				if (RightMousePressed())
					{ selCol = c; g_vCLUTs[selCLUT].channels[c] = -1; }
			}
		}

		// **************************************
		//  Process keyboard input
		// **************************************

		char key = LastKey();
		static string texturePath = folderPath +string("\\..\\Textures");

		switch (key)
		{
			case 'x': bExit = true; break;
			case 'm': 
				bUpdateSegments = true; 
				theMode = MODE_MAP;
				break;
			case 't': 
				LoadPNGThumbnails(texturePath);
				bUpdateTextures = true; 
				theMode = MODE_TEXTURES;
				break;

			case Left: if (xscroll == 1024) { xscroll = 0; } theMode==MODE_MAP ? bUpdateSegments=true : bUpdateTextures = true; break;
			case Right: if (xscroll == 0) { xscroll = 1024; } theMode == MODE_MAP ? bUpdateSegments = true : bUpdateTextures = true; break;
			case Up: if (yscroll == 1024) { yscroll = 0; } theMode == MODE_MAP ? bUpdateSegments = true : bUpdateTextures = true; break;
			case Down: if (yscroll == 0) { yscroll = 1024; } theMode == MODE_MAP ? bUpdateSegments = true : bUpdateTextures = true; break;

			case 's': 
				SaveChannelPNGs(folderPath, pSegmentData, pTIMData);
				break;
		}

		if (prevSelCLUT != selCLUT)
			{ bUpdateCLUTs = true; bUpdateColours = true; }

		if (prevSelCol != selCol)
			{ bUpdateColours = true; bUpdateCLUTs = true; }

		if (prevSelChan != selChan)
			bUpdateChannels = true;

		if (selTexture != prevSelTexture)
			bUpdateTextures = true;

		if (lightTexture != prevLightTexture)
			bUpdateTextures = true;




		// ***********************************************
		//  Refresh screen when all TIMs change
		// ***********************************************
		if (bUpdateTIMs)
		{
			// Draw all the TIMs again
			for (int t = 0; t < timCount; t++)
				DrawTIM(pTIMData[t], _TIMXPOS(t), _TIMYPOS(t));

			// Draw all the CLUTs again
			for (int u = 0; u < uniqueCLUTs; u++)
				DrawTIM(pTIMData[g_vCLUTs[u].sharedRefs[0]], _CLUTXPOS(u), _CLUTYPOS(u));
		}

		// **************************************
		//  Refresh screen when CLUTs change
		// **************************************

		if (bUpdateCLUTs)
		{
			// Remove outlines from previously highlighted TIMs
			for (int t : g_vCLUTs[prevSelCLUT].sharedRefs)
				DrawRectangle(_TIMXPOS(t) - 1, _TIMYPOS(t) - 1, 34, 34, MakeColor(0, 0, 0), false);

			// Add outline to newly highlighted TIMs
			for (int t : g_vCLUTs[selCLUT].sharedRefs)
				DrawRectangle(_TIMXPOS(t) - 1, _TIMYPOS(t) - 1, 34, 34, MakeColor(255, 255, 0), false);

			// Update outlines of previously and currently selected CLUTs
			DrawRectangle(_CLUTXPOS(prevSelCLUT) - 1, _CLUTYPOS(prevSelCLUT) - 1, 34, 34, MakeColor(0, 0, 0), false);
			DrawRectangle(_CLUTXPOS(selCLUT) - 1, _CLUTYPOS(selCLUT) - 1, 34, 34, MakeColor(255, 255, 0), false);

			// Redraw the TIM of the previous selected CLUT and put a black square in the TIM of the currently selected CLUT
			DrawTIM(pTIMData[g_vCLUTs[prevSelCLUT].sharedRefs[0]], _CLUTXPOS(prevSelCLUT), _CLUTYPOS(prevSelCLUT));
			DrawRectangle(_CLUTXPOS(selCLUT) + 11, _CLUTYPOS(selCLUT) + 11, 10, 10, MakeColor(0, 0, 0), true);

			// Display how many TIMs share this CLUT
			DrawRectangle(6, timBase + 4, 8 * 32, 32, MakeColor(0, 0, 0), true);
			DrawString(6, timBase + 4, to_string(g_vCLUTs[selCLUT].sharedRefs.size()) + " selected TIMs", MakeColor(255, 255, 255));

			// Redraw the colours in the CLUT
			for (int c = 0; c < 16; c++)
				DrawRectangle(_COLXPOS(c) - 1, _COLYPOS(c) - 1, 33, 33, MakeColor(0, 0, 0), false);

			for (int c = 0; c < 16; c++)
				DrawRectangle(_COLXPOS(c), _COLYPOS(c), 31, 31, GetOriginalCLUTcolour(g_vCLUTs[selCLUT], c), true);

			prevSelCLUT = selCLUT;
		}

		// ***********************************************
		//  Refresh screen when CLUT colours change
		// ***********************************************
		if (bUpdateColours)
		{
			DrawString(273, g_clutBase + 14, "16 colours", MakeColor(255, 255, 255));

			for (int c = 0; c < 16; c++)
				DrawRectangle(_COLXPOS(c), _COLYPOS(c), 31, 31, GetOriginalCLUTcolour(g_vCLUTs[selCLUT], c), true);

			for (int c = 0; c < 16; c++)
			{
				Color col = MakeColor(0, 0, 0);

				if (g_vCLUTs[selCLUT].channels[c] != -1)
				{
					col = g_chanCol[g_vCLUTs[selCLUT].channels[c]];
					DrawRectangle(_COLXPOS(c) + 11, _COLYPOS(c) + 11, 10, 10, col, true);
				}

				DrawRectangle(_COLXPOS(c) - 1, _COLYPOS(c) - 1, 33, 33, col, false);
			}

			for (int t : g_vCLUTs[prevSelCLUT].sharedRefs)
				DrawTIM(pTIMData[t], _TIMXPOS(t), _TIMYPOS(t));

			prevSelCol = selCol;
		}



		// ***********************************************
		//  Refresh screen when selected channel changes
		// ***********************************************
		if ( bUpdateChannels )
		{
			DrawString(273, g_clutBase + 112, "9 channels", MakeColor(255, 255, 255));

			for (int c = 0; c<SPLAT_CHANNELS; c++)
				DrawRectangle(_CHXPOS(c), _CHYPOS(c), 32, 32, g_chanCol[c], true);

			DrawRectangle(_CHXPOS(prevSelChan) - 1, _CHYPOS(prevSelChan) - 1, 34, 34, MakeColor(0, 0, 0), false);
			DrawRectangle(_CHXPOS(selChan) - 1, _CHYPOS(selChan) - 1, 34, 34, MakeColor(255, 255, 255), false);
			DrawRectangle(_CHXPOS(selChan) + 11, _CHYPOS(selChan) + 11, 10, 10, MakeColor(0, 0, 0), true);

			prevSelChan = selChan;
		}


		string coords = "[" + to_string(xscroll+1024) + "," + to_string(yscroll) + "]";
		int swidth = MeasureString(coords);

		// ***********************************************
		//  Refresh screen MAP segment image changes
		// ***********************************************
		if (bUpdateSegments == true && theMode == MODE_MAP)
		{
			DrawSegments2Buffer(pSegmentData);
			BlitPixels(g_pMapBuffer, BUFX, BUFY, xscroll, yscroll, 1024, 1024, 2048);

			DrawRectangle(1300, 3, 260, 20, MakeColor(0, 0, 0), true);
			DrawString(1560-swidth-4, 4, coords, MakeColor(255, 255, 255));
		}


		// ***********************************************
		//  Refresh texture select changes
		// ***********************************************
		if (bUpdateTextures == true && theMode == MODE_TEXTURES )
		{
			BlitPixels(g_pThumbBuffer, BUFX, BUFY, xscroll, yscroll, 1024, 1024, 2048);

			int relXPos =  _TEXTUREXPOS(selTexture);
			int relYPos =  _TEXTUREYPOS(selTexture);

			if (relXPos >= xscroll && relXPos < xscroll + 1024 && relYPos >= yscroll && relYPos < yscroll + 1024)
			{
				relXPos = relXPos % 1024;
				relYPos = relYPos % 1024;
				DrawRectangle(BUFX + relXPos, BUFY + relYPos, 64, 64, MakeColor(255, 255, 255), false);
				DrawRectangle(BUFX + relXPos + 17, BUFY + relYPos + 17, 30, 30, MakeColor(0, 0, 0), true);
			}

			prevSelTexture = selTexture;

			if (lightTexture != -1 )
			{
				relXPos = _TEXTUREXPOS(lightTexture);
				relYPos = _TEXTUREYPOS(lightTexture);

				if (relXPos >= xscroll && relXPos < xscroll + 1024 && relYPos >= yscroll && relYPos < yscroll + 1024)
				{
					relXPos = relXPos % 1024;
					relYPos = relYPos % 1024;

					string s = "< >";

					if( lightTexture < g_vTextures.size() )
						s = "< " + g_vTextures[lightTexture] + " >";

					int w = MeasureString(s);
					int xClamp = (relXPos - (w / 2) + 32);
					if (xClamp < 0) xClamp = 0;
					if (xClamp + w > 1024) xClamp = 1024 - w;
					DrawRectangle(BUFX + xClamp, BUFY + relYPos + 32, w, 24, MakeColor(0, 0, 0), true);
					DrawString(BUFX + xClamp, BUFY + relYPos + 32 + 4, s, MakeColor(255, 255, 255));
				}
			}

			prevLightTexture = lightTexture;

			DrawRectangle(1300, 3, 260, 20, MakeColor(0, 0, 0), true);
			DrawString(1560 - swidth-4, 4, coords, MakeColor(255, 255, 255));
		}

		frame++;

	}

	CloseWindow();

	delete pTIMData;
	delete pSegmentData;
	delete g_pMapBuffer;
	delete g_pThumbBuffer;

	return 0;
} // Happily ever after


// ***************************************************************************************************
// **************************************  FUNCTION DEFINITIONS **************************************
// ***************************************************************************************************

// Description: Used to display fatal errors on initialisation
int FatalError(const char* message)
{
	cout << message << endl;
	DrawString(6, 4, message, MakeColor(255, 0, 0));
	return -1;
}

// Description: Used to display safe errors during tool use
int Error(const char* message)
{
	cout << message << endl;
	DrawString(MESSX, MESSY, message, MakeColor(255, 0, 0));
	return -1;
}


// Function:	DrawTIM
// Description: Draws a 4-bit TIM into the 32-bit display buffer one pixel at a time!
// Notes:		Could be seriously optimised by converting all TIMs to 32-bit in advance and blitting across!
int DrawTIM(TIM_FILE& timData, int x, int y)
{
	for (int dy = 0; dy < 32; dy++)
	{
		for (int dx = 0; dx < 32; dx++)
		{
			int pixelIndex = (dy * 8) + (int)floor(dx / 4);
			int colourIndex = timData.pixel.pixelData[pixelIndex];
			colourIndex = (colourIndex>> ((dx % 4) * 4)) & 0xF;
			Color col = GetCLUTcolour(timData.clut, colourIndex);
			SetPixel(x + dx, y + dy, col );
		}
	}
	return 0;
}


// Function:	GetCLUTcolour
// Description: The program allows the user to "override" specific palette entries in the list of shared CLUTs, so this function
//				searches the global list of shared CLUTs for a specific CLUT and returns any overriding colour for a given entry.
// Arguments:	clut - a 16-colour CLUT (from a TIM)
//				index - index to the specific palette entry from that CLUT (0-15)
// Returns:		The colour (either the original or an override)
Color GetCLUTcolour(CLUT_BLOCK& clut, int index)
{
	int red	= (clut.clutData[index])&0x001F;
	int green = (clut.clutData[index]>>5) & 0x001F;
	int blue = (clut.clutData[index]>>10) & 0x001F;
	
	// Cluts are only 5-bits per channel, hence they need to be scaled
	Color col = MakeColor(red*8, green*8, blue*8);

	bool bMatched;
	for (SHARED_CLUT& sclut : g_vCLUTs)
	{
		bMatched = true;
		for (int c = 0; c < 16; c++)
		{
			if (sclut.clutData[c] != clut.clutData[c])
				bMatched = false;
		}

		if (bMatched)
		{
			if (sclut.channels[index] != -1)
				col = g_chanCol[sclut.channels[index]];
		}
	}

	return col;
}

// Function:	GetOriginalCLUTcolour
// Description: Gets a colour from a shared CLUT, but without looking for overrides
Color GetOriginalCLUTcolour(SHARED_CLUT& clut, int index)
{
	int red = (clut.clutData[index]) & 0x001F;
	int green = (clut.clutData[index] >> 5) & 0x001F;
	int blue = (clut.clutData[index] >> 10) & 0x001F;

	// Cluts are only 5-bits per channel, hence they need to be scaled
	return MakeColor(red * 8, green * 8, blue * 8);
}

// Function:	UnmanglePTM
// Description: Creates an array of TIM texture structures from a mangled file of Hogs level texture TIMs (.PTM)
// Arguments:	filename - pointer to c-style string indicating the PTM filename
//				pTims (returned) - pointer to an array of TIMs allocated by the function
// Returns:		Number of TIMs in PTM (up to 256)
// Notes:		pTims is passed by reference; function allocated memory, but it is up to the calling program to delete!
int UnmanglePTM(char* filename, TIM_FILE* &pTims )
{
	ifstream inputFile;

	// Load the mangled file into an appropriately sized buffer
	inputFile.open(filename, ios::binary | ios::in);
	inputFile.seekg(0, std::ios::end);
	int fileSize = (int)inputFile.tellg();
	char* pInputBuffer = new char[fileSize];
	inputFile.seekg(0, std::ios::beg);
	inputFile.read(pInputBuffer, fileSize);
	inputFile.close();

	// Unmange the mangled file into an disproportionately larger buffer (we have no idea how big it will be!)
	unsigned char* pUnmangleBuffer = new unsigned char[1000000];
	int unmangledBufferSize = Unmangle(pUnmangleBuffer, (unsigned char*)pInputBuffer);
	int timCount = *((unsigned int*)pUnmangleBuffer); // first 4 bytes is the TIM count
	// Copy to a perfectly sized array of structures
	pTims = new TIM_FILE[timCount];
	memcpy(pTims, (char*)&pUnmangleBuffer[4], sizeof(TIM_FILE)*timCount);

	delete pUnmangleBuffer;
	delete pInputBuffer;
	   
	return timCount; // up to calling program to delete memory!
}

// Function:	UnmanglePMM
// Description: Creates an array of SEGMENT texture structures from a mangled file of Hogs (height)map data (.PMM)
// Arguments:	filename - pointer to c-style string indicating the PTM filename (from which the PMM is inferred)
//				pSements (returned) - pointer to an array of SEGMENTs allocated by the function
// Returns:		Number of SEGMENTs in PTM (should be 16x16=256)
// Notes:		pTims is passed by reference; function allocated memory, but it is up to the calling program to delete!
int UnmanglePMM(char* filename, SEGMENT* &pSegments)
{
	string newFile = string(filename);
	newFile.replace(newFile.find(string(".PTM")), 4, string(".PMM"));

	ifstream inputFile;

	// Load the mangled file into an appropriately sized buffer
	inputFile.open(newFile.c_str(), ios::binary | ios::in);
	if (!inputFile.is_open()) return -1;
	inputFile.seekg(0, std::ios::end);
	int fileSize = (int)inputFile.tellg();
	char* pInputBuffer = new char[fileSize];
	inputFile.seekg(0, std::ios::beg);
	inputFile.read(pInputBuffer, fileSize);
	inputFile.close();

	// Unmange the mangled file into an disproportionately larger buffer (we have no idea how big it will be!)
	unsigned char* pUnmangleBuffer = new unsigned char[1000000];
	int unmangledBufferSize = Unmangle(pUnmangleBuffer, (unsigned char*)pInputBuffer);
	int segmentCount = unmangledBufferSize / sizeof(SEGMENT); 
	// Copy to a perfectly sized array of structures
	pSegments = new SEGMENT[segmentCount];
	memcpy(pSegments, (char*)pUnmangleBuffer, sizeof(SEGMENT)*segmentCount);
	
	delete pUnmangleBuffer;
	delete pInputBuffer;

	return segmentCount; // up to calling program to delete memory!
}


// Function:	CalculateUniqueCLUTs
// Description: Creates a vector of structures containing the unique CLUTs and (another vector) of indexes to all 
//				the other TIMs which share that CLUT
// Arguments:	pTims - pointer to the array of TIMs
//				timCount - the number of TIMs in the array
//				vCLUTS - reference to an empty vector of shared CLUTs (added to be the function)
// Returns:		Number of unique CLUTs in the array of TIMs
int CalculateUniqueCLUTs(TIM_FILE* pTims, int timCount, vector<SHARED_CLUT> &vCLUTs)
{
	// First CLUT will always be unique so add it
	SHARED_CLUT firstCLUT;
	memcpy(firstCLUT.clutData, pTims[0].clut.clutData, sizeof(short) * 16);
	firstCLUT.sharedRefs.push_back(0);
	vCLUTs.push_back(firstCLUT);

	int uniqueCLUTs = 1;

	for (int t = 1; t < timCount; t++)
	{
		CLUT_BLOCK timCLUT = pTims[t].clut;
		
		bool bMatched;

		for (SHARED_CLUT& clut : vCLUTs)
		{
			bMatched = true;
			for (int col = 0; col < 16; col++)
			{
				if (clut.clutData[col] != timCLUT.clutData[col])
					bMatched = false;
			}

			if (bMatched)
			{
				clut.sharedRefs.push_back(t);
				break;
			}
		}

		if( !bMatched )
		{
			SHARED_CLUT newCLUT;
			memcpy(newCLUT.clutData, pTims[t].clut.clutData, sizeof(short) * 16);
			newCLUT.sharedRefs.push_back(t);
			vCLUTs.push_back(newCLUT);
			
			uniqueCLUTs++;
		}
	}

	return uniqueCLUTs;
}


// Sets a pixel within the map display buffer (2048 x 2048 pixels @ 32-bit)
void SetBufferPixel(int x, int y, Color c)
{
	g_pMapBuffer[(y * 2048) + x] = c;
}

// Function:	CopyTIM2Buffer
// Description: Copies a TIM from the display buffer to the map buffer in the correct orientation
// Arguments:	sourcex/sourcey - the top left pixel in the display buffer to copy from 
//				destx/desty - the top left pixel in the map buffer to copy to 

int CopyTIM2Buffer(int sourcex, int sourcey, int destx, int desty, int rot)
{
	// TO DO: Implement this function (see slides)


	return 0;
}

// Function:	DrawSegments2Buffer
// Description: Draws a representation of the orginal level to the map display buffer 
// Arguments:	pSegments - pointer to the array of SEGMENT data representing the map
//				pTIMData - pointer to the array of TIM data storing the textures
// Notes:		Pulls the TIM data from the display buffer as it is already in the correct bit depth
//				(32-bit) and copying it automatically propogates any colour changes to the map
int DrawSegments2Buffer(SEGMENT* pSegments)
{
	// TO DO: Implement this function (see slides)
	// Note the code below should copy the TIM at index "tileIndex" to the map grid square "mapIndex" 
	// CopyTIM2Buffer(_TIMXPOS(tileIndex), _TIMYPOS(tileIndex), _MAPXPOS(mapIndex), _MAPYPOS(mapIndex), tileRot);

	for (int i = 0; i < 256; i++) {
		for (int poly = 0; poly < 16; poly++) {
			POLYSTRUCT polystruct = pSegments->strTilePolyStruct[i];
		}
	}

//function to find the right tile somewhere!?!?!
	return 0;
}


// Function:	SaveDiffusePNG
// Description: Saves a "diffuse map" representation of the orginal level to a PNG format file
// Arguments:	folderPath - reference to string defining where to put it
//				pSegmentData - opointer to the SEGMENT data storing the map data
//				pTIMData - texture data for drawing the level map
// Returns:		The result of SaveNewBitmap2PNG()
int SaveDiffusePNG(string &folderPath, SEGMENT* pSegmentData, TIM_FILE* pTIMData)
{
	string fileAndPath = folderPath + string("\\DiffuseMap.png");

	DrawString(MESSX, MESSY, string("Saving file: ") + fileAndPath, MakeColor(255, 255, 255));

	DrawSegments2Buffer(pSegmentData);

	int ret = SaveNewBitmap2PNG(g_pMapBuffer, fileAndPath, 2048, 2048);

	DrawRectangle(MESSX, MESSY-1, 1024, 32, MakeColor(0, 0, 0), true);

	return ret;
}


// Function:	SaveChannelPNGs
// Description: Saves a set of 9 PNGs representing splat map data for the level [1=RGB, 2=RGB, 3=RGB]
// Arguments:	folderPath - reference to string defining where to put it
//				pSegmentData - opointer to the SEGMENT data storing the map data
//				pTIMData - texture data for drawing the level map
// Returns:		1 for success
int SaveChannelPNGs(string &folderPath, SEGMENT* pSegmentData, TIM_FILE* pTIMData )
{
	DrawSegments2Buffer(pSegmentData);

	uint32_t* pChannelBuffer = new uint32_t[2048 * 2048];

	uint32_t *pSource, *pDest;
	
	for (int chan = 0; chan < 9; chan++)
	{
		string fileAndPath = folderPath + string("\\Channel_") + to_string(chan) + ".png";
		DrawString(MESSX, MESSY, string("Saving: ") + fileAndPath, MakeColor(255, 255, 255));

		memset(pChannelBuffer, 0, 2048 * 2048 * 4);

		pSource = g_pMapBuffer;
		pDest = pChannelBuffer;
		Color col = MakeColor(255 * (chan % 3 == 0), 255 * (chan % 3 == 1), 255 * (chan % 3 == 2));

		*pDest = col; // Always write first pixel so layer is never blank (helps with Photoshop)

		for (int pixel = 0; pixel < 2048 * 2048; pixel++)
		{
			if (*pSource++ == g_chanCol[chan])
				*pDest = col;

			pDest++;
		}

		if( SaveNewBitmap2PNG(pChannelBuffer, fileAndPath, 2048, 2048) != 1 )
			return Error("Error: Unable to save channel PNGs.");

		DrawRectangle(MESSX, MESSY-1, 1024, 32, MakeColor(0, 0, 0), true);
	}

	SaveCLUTChannelData(folderPath);

	delete pChannelBuffer;

	return 1;
}

// Function:	LoadCLUTChannelData
// Description: Reloads the saved shared CLUT data and channel overrides for this level
// Arguments:	folderPath - reference to string defining where to find it
// Returns:		1 for success
int LoadCLUTChannelData(string &folderPath) 
{ 
	ifstream inputFile;

	string filename = folderPath + "\\splat.sav";

	inputFile.open(filename, ios::binary | ios::in);

	if( inputFile.fail())
		return Error("Error: Unable to restore splat file.");
	
	for (SHARED_CLUT& clut : g_vCLUTs)
	{
		for( int col=0; col<16; col++ )
			inputFile.read((char*)&clut.channels[col], sizeof(int));
	}

	inputFile.close();

	return 1; 
}

// Function:	SaveCLUTChannelData
// Description: Saves the shared CLUT data and channel overrides for this level
// Arguments:	folderPath - reference to string defining where to put it
// Returns:		1 for success
int SaveCLUTChannelData(string &folderPath) 
{ 
	ofstream outputFile;

	string filename = folderPath + "\\splat.sav";

	outputFile.open(filename, ios::binary | ios::out);

	if (outputFile.fail())
		return Error("Error: Unable to save splat file.");

	for (SHARED_CLUT& clut : g_vCLUTs)
	{
		for (int col = 0; col < 16; col++)
			outputFile.write((char*)&clut.channels[col], sizeof(int));
	}

	outputFile.close();

	return 1;
}

// Function:	LoadChannelThumbnails
// Description: Loads the high-res textures associated with each channel from the level dir
// Arguments:	folderPath - reference to string defining the level dir
// Returns:		Number of textures found
// TODO:		Just displays them for now - needs logic to move and rename them from the Textures dir as they are
int LoadChannelThumbnails(string &folderPath)
{
	std::string ext = ".png";

	int x = 0, y = 0, count = 0;

	uint32_t* thumbBits = new uint32_t[64 * 64];

	for (auto & p : experimental::filesystem::directory_iterator(folderPath))
	{
		string filename = p.path().string();

		if (p.path().extension() == ext && (filename.find("Base") != string::npos))
		{
			DrawRectangle(MESSX, MESSY - 1, 1024, 32, MakeColor(0, 0, 0), true);
			DrawString(MESSX, MESSY, string("Loading: ") + filename, MakeColor(255, 255, 255));

			uint32_t* thumbBitsIt = thumbBits;

			LoadPNGThumbBits(filename, 64, 64, thumbBits);

			x = _CHTEXTXPOS(count);
			y = _CHTEXTYPOS(count);

			for (int dy = y; dy < y + 64; dy++)
			{
				for (int dx = x; dx < x + 64; dx++)
				{
					SetPixel(dx, dy, *thumbBitsIt++);
				}
			}

			count++;
		}
	}

	DrawString(273, g_clutBase + 247, to_string(count) + " textures", MakeColor(255, 255, 255));

	delete thumbBits;

	DrawRectangle(MESSX, MESSY - 1, 1024, 32, MakeColor(0, 0, 0), true);
	DrawString(MESSX, MESSY, string("Successfully loaded ") + to_string(count) + " textures.", MakeColor(255, 255, 255));

	return count;
}

// Function:	RestorePNGThumbnails
// Description: Looks for a pre-existing set of thumbnails in the textures directory
// Arguments:	folderPath - reference to string defining the level dir
// Returns:		Number of textures (re)found
int RestorePNGThumbnails(string &folderPath)
{
	bool unchanged = true;
	std::string ext = ".png";
	std::time_t timestamp;
	int totalFiles = 0, files = 0;

	ifstream inputFile;

	string filename = folderPath + "\\.Versions.inf";

	inputFile.open(filename, ios::in);

	if (inputFile.fail())
		return Error("Unable to restore file versions file."); // May not be an error first time through

	inputFile >> totalFiles;

	for (auto & p : experimental::filesystem::directory_iterator(folderPath))
	{
		filename = p.path().string();

		if (p.path().extension() == ext && (filename.find("Base") != string::npos))
		{
			auto ftime = experimental::filesystem::last_write_time(p);
			std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);

			g_vTextures.push_back(p.path().filename().string());

			inputFile >> filename;
			inputFile >> timestamp;

			if (p.path().filename() != filename)
				unchanged = false;

			if (timestamp != cftime)
				unchanged = false;

			files++;
		}
	}

	if( files != totalFiles )
		unchanged = false;

	if (!unchanged)
		return false; // Needs updating

	filename = folderPath + "\\.Thumbnails.png";

	LoadPNGBits(filename, 2048, 2048, g_pThumbBuffer);

	DrawRectangle(MESSX, MESSY-1, 1024, 32, MakeColor(0, 0, 0), true);
	DrawString(MESSX, MESSY, string("Successfully restored ") + to_string(files) + " textures.", MakeColor(255, 255, 255));

	return true;
}

// Function:	LoadPNGThumbnails
// Description: Creates a set of thumbnails from the textures directory
// Arguments:	folderPath - reference to string defining the textures dir
// Returns:		Number of textures found
int LoadPNGThumbnails(string &folderPath)
{
	g_vTextures.clear();

	if( RestorePNGThumbnails(folderPath) > 0 )
		return 1;

	ofstream outputFile;

	string filename = folderPath + "\\.Versions.inf";

	outputFile.open(filename, ios::out);

	if (outputFile.fail())
		return Error("Error: Unable to save texture versions file.");

	std::string ext = ".png";

	int x = 0, y = 0, count = 0;

	memset(g_pThumbBuffer, 0xFFFFFFFF, 2048 * 2048 * sizeof(int));

	int files = 0;
	for (auto & p : experimental::filesystem::directory_iterator(folderPath))
	{
		string filename = p.path().string();
		if (p.path().extension() == ext && (filename.find("Base") != string::npos))
			files++;
	}

	outputFile << to_string(files) << "\n";
	
	uint32_t* thumbBits = new uint32_t[64 * 64];

	for (auto & p : experimental::filesystem::directory_iterator(folderPath))
	{
		string filename = p.path().string();

		if (p.path().extension() == ext && (filename.find("Base") != string::npos))
		{
			auto ftime = experimental::filesystem::last_write_time(p);
			std::time_t cftime = decltype(ftime)::clock::to_time_t(ftime);
			outputFile << p.path().filename() << " " << to_string(cftime) << '\n';
			g_vTextures.push_back(p.path().filename().string());
		
			DrawRectangle(MESSX, MESSY-1, 1024, 32, MakeColor(0, 0, 0), true);
			DrawString(MESSX, MESSY, string("Loading: ") + filename, MakeColor(255, 255, 255));

			uint32_t* thumbBitsIt = thumbBits;

			LoadPNGThumbBits(filename, 64, 64, thumbBits);

			x = _TEXTUREXPOS(count);
			y = _TEXTUREYPOS(count);

			for (int dy = y; dy < y + 64; dy++)
			{
				for (int dx = x; dx < x + 64; dx++)
				{
					g_pThumbBuffer[(dy * 2048) + dx] = *thumbBitsIt++;
				}
			}

			count++;
		}
	}

	delete thumbBits;

	string fileAndPath = folderPath + string("\\.Thumbnails.png");

	DrawString(MESSX, MESSY, string("Saving thumbnail... ") + fileAndPath, MakeColor(255, 255, 255));

	int ret = SaveNewBitmap2PNG(g_pThumbBuffer, fileAndPath, 2048, 2048);

	DrawRectangle(MESSX, MESSY-1, 1024, 32, MakeColor(0, 0, 0), true);
	DrawString(MESSX, MESSY, string("Successfully loaded ") + to_string(count) + " textures.", MakeColor(255, 255, 255));

	outputFile.close();

	return count;
}




// Function:	Unmangle
// Description: The original data decompression function used by Gremlin Graphics in many of their games
// Arguments:	dest - pointer to a buffer to put the decompressed data
//				source - pointer to the compressed data
// Returns:		Size of of decompressed data
// Notes:		Unfortunately there is no way of knowing in advance how big the decompressed data is...
//				Could write an UnmangleSize function that decompressed without writing?
int Unmangle(unsigned char *dest, unsigned char *source)
{
	int c;
	int u, v;
	unsigned char *pt;
	unsigned char *start = dest;

	while ((c = *source++) != 0)
	{
		if (c & 128)
		{
			// Bit 7 : 1 : BLOCK
			if (c & 64)
			{
				// Bit 6 : 1 Block size
				if (c & 32)
				{
					// Bit 5 : 1 Long
					pt = dest - ((c & 31) << 8) - *source++ - 3;	// offset
					c = *source++ + 5;						// size
					while (c--)
						*dest++ = *pt++;
				}
				else
				{
					// Bit 5 : 0 Medium
					pt = dest - ((c & 3) << 8) - *source++ - 3;	// offset
					c = ((c >> 2) & 7) + 4;					// size
					while (c--)
						*dest++ = *pt++;

				}
			}
			else
			{
				// Bit 6 : 0 : Short
				pt = dest - (c & 63) - 3;
				*dest = *pt;
				*(dest + 1) = *(pt + 1);
				*(dest + 2) = *(pt + 2);
				dest += 3;
			}
		}
		else
		{
			// Bit 7 : 0
			if (c & 64)
			{
				// Bit 6 : 1 Seq/Diff or String
				if (c & 32)
				{
					// Bit 5 : 1 : Sequence
					if (c & 16)
					{
						// Bit 4 : 1 : Word sequence
						c = (c & 15) + 2;	// bits 3-0 = len 2->17
						v = *(short *)(dest - 2);
						while (c--)
						{
							*(short *)dest = (short)v;
							dest += 2;
						}
					}
					else
					{
						// Bit 4 : 0 : Byte sequence
						c = (c & 15) + 3;	// bits 3-0 = len 3->18
						v = *(dest - 1);
						while (c--)
							*dest++ = (signed char)v;
					}
				}
				else
				{
					// Bit 5 : 0 : Difference
					if (c & 16)
					{
						// Bit 4 : 1 : Word difference
						c = (c & 15) + 2;			// bits 3-0 = len 2->17
						u = *(short *)(dest - 2);		// start word
						v = u - *(short *)(dest - 4);	// dif
						while (c--)
						{
							u += v;
							*(short *)dest = (short)u;
							dest += 2;
						}
					}
					else
					{
						// Bit 4 : 0 : Byte difference
						c = (c & 15) + 3;	// bits 3-0 = len 3->18
						u = *(dest - 1);		// start byte
						v = u - *(dest - 2);	// dif

						while (c--)
						{
							u += v;
							*dest++ = (char)u;
						}
					}
				}
			}
			else
			{
				// Bit 6 : 0 : String
				c &= 63;	// len
				memcpy(dest, source, c);
				dest += c;
				source += c;
			}
		}
	}
	return (int)(dest - start);
}