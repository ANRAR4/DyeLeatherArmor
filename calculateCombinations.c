#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <malloc.h>
#include <signal.h> 

#define MAX_AB(a, b) (a > b ? a : b)

// Number of unique possible dyecombinations in 3*3 crafting grid
#define COLOUROPTIONSCOUNT 735470L
// Number of possible dyes per crafting step
#define COLOURCOUNT 8
// Basecolours to test per thread per step
#define BASECOLOURSPERTHREAD 30L

// parameters for each thread
struct threadParams {
	int32_t* colourBase;
	uint8_t* colourOptions;
	uint64_t* colourOptionsInt;
	uint32_t* coloursFoundBool;
	struct colourMix* coloursFound;
	uint32_t coloursFoundCount;
	uint32_t colourBaseOffset;
	uint32_t colourBaseCount;
};

// struct to describe a crafting step(colour as resulting colour, armorBaseColour as current colour, newColours as list of added dyes)
struct colourMix {
	uint32_t colour;
	int32_t armorBaseColour;
	uint64_t newColours;
};

// struct to hold information about one rgb colour
struct rgb_colour {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

static char receivedSignal;

// Signal(Strg + C) handler, used to guarantee consistency when the programm is terminated
void exit_after_finishing(int sig)
{
    signal(SIGINT, exit_after_finishing);
    printf("Stopping programm!\n");    
    receivedSignal = 1;
}

// function to get the rgb values for an integer
struct rgb_colour coloursForInt(int baseColour) {
	struct rgb_colour colours;

	colours.red = (uint8_t)(baseColour & 16711680) >> 16;
	colours.green = (uint8_t)(baseColour & 65280) >> 8;
	colours.blue = (uint8_t)(baseColour & 255) >> 0;

	return colours;
}

// For use with a version other then 1.12 or above the according array must be commented in

// // Colourcodes to use for craftingrecipes in version 1.4.2
// // 1.4.2
// const int32_t elementaryColours[16][3] = {{25, 25, 25},
// {204, 76, 76},
// {102, 127, 51},
// {127, 102, 76},
// {51, 102, 204},
// {178, 102, 229},
// {76, 153, 178},
// {153, 153, 153},
// {76, 76, 76},
// {242, 178, 204},
// {127, 204, 25},
// {229, 229, 51},
// {153, 178, 242},
// {229, 127, 216},
// {242, 178, 51},
// {255, 255, 255}};

// // Colourcodes to use for craftingrecipes in versions 1.4.2 to 1.11.2
// // 1.4.3-1.11
// const int32_t elementaryColours[16][3] = {{25, 25, 25},
// {153, 51, 51},
// {102, 127, 51},
// {102, 76, 51},
// {51, 76, 178},
// {127, 63, 178},
// {76, 127, 153},
// {153, 153, 153},
// {76, 76, 76},
// {242, 127, 165},
// {127, 204, 25},
// {229, 229, 51},
// {102, 153, 216},
// {178, 76, 216},
// {216, 127, 51},
// {255, 255, 255}};

// Colourcodes to use for craftingrecipes in version 1.12 and above
// 1.12
const int32_t elementaryColours[16][3] = {{29, 29, 33},
{176, 46, 38},
{94, 124, 22},
{131, 84, 50},
{60, 68, 170},
{137, 50, 184},
{22, 156, 156},
{157, 157, 151},
{71, 79, 82},
{243, 139, 170},
{128, 199, 31},
{254, 216, 61},
{58, 179, 218},
{199, 78, 189},
{249, 128, 29},
{249, 255, 254}};

// function to calculate the resulting colour from armorcolour as current base and dyes as dyes to add
// equivalent to the function used inside minecraft
int colourForCombination(uint8_t* dyes, int32_t armorcolour) {
	int maxSum = 0, itemCount = 0, i;
	int red, green, blue, redSum = 0, greenSum = 0, blueSum = 0;
	float average, avgMax;
	uint32_t returnColour;

	if (armorcolour > -1) {
		redSum = (armorcolour >> 16) & 255;
		greenSum = (armorcolour >> 8) & 255;
		blueSum = armorcolour & 255;
		maxSum = MAX_AB(redSum, MAX_AB(greenSum, blueSum));
		itemCount++;
	}

	for (i = 0; i < COLOURCOUNT; i++) {
		if (dyes[i] > 0) {
			red = elementaryColours[dyes[i] - 1][0];
			green = elementaryColours[dyes[i] - 1][1];
			blue = elementaryColours[dyes[i] - 1][2];
			maxSum += MAX_AB(red, MAX_AB(green, blue));
			redSum += red;
			greenSum += green;
			blueSum += blue;
			itemCount++;
		}
	}

	red = (int)(redSum / itemCount);
	green = (int)(greenSum / itemCount);
	blue = (int)(blueSum / itemCount);
	average = (float)maxSum / (float)itemCount;
	avgMax = (float)MAX_AB(red, MAX_AB(green, blue));
	red = (int)((float)red * average / avgMax);
	green = (int)((float)green * average / avgMax);
	blue = (int)((float)blue * average / avgMax);
	returnColour = (int)(red << 8) + green;
	returnColour = (int)(returnColour << 8) + blue;

	return returnColour;
}

// function to transform an integer listing dyes(5 bit per colour(0 = no colour, 1 = black, 16 = white etc.)) to an array with an element for each dye
void getDyesForInt(uint64_t dyeint, uint8_t* dyes) {
	int i;

	for (i = 0; i < COLOURCOUNT; i++) {
		dyes[i] = dyeint & 0x1F;
		dyeint = dyeint >> 5;
	}
}

void* test_dyecombs(void* args);

int main(int argc, char** args)
{
	int threads, returnCode, returnVal;
	size_t len = 0, read;
	pthread_t* tid;
	FILE *fp, *fpProgress, *fpLayer;
	size_t i, i2, iB;
	struct threadParams* params;
	uint32_t* coloursFoundBool;

	uint64_t colour;
	struct colourMix colourmix;
	struct colourMix* coloursFoundByThreads;
	struct tm* t;
	time_t start, stop;

	// version combinations are calculated for(used for filenames)
	const char version[] = "1-12";

	int32_t progress = 0, layerCount = 0;

	if (argc != 2)
	{
		printf("Usage: %s <Number of Threads>\n", args[0]);
		return 0;
	}

	mallopt(M_MMAP_THRESHOLD, 4 * 1024 * 1024 * sizeof(long));

	threads = atoi(args[1]);
    if (threads <= 0)
	{
		printf("Threadcount needs to be > 0!\n");
		return 0;
	}

	int32_t* newArmorBase;
	uint32_t* nextArmorBase;
	uint64_t* colourOptionsInt;
	uint8_t* colourOptions;
	uint8_t* currentDyes;
	char* line = NULL;

	uint32_t nextArmorBaseCount, newArmorBaseCount;

	colourOptionsInt = (uint64_t*)calloc(COLOUROPTIONSCOUNT, sizeof(uint64_t));
	colourOptions = (uint8_t*)calloc(COLOUROPTIONSCOUNT * COLOURCOUNT, sizeof(uint8_t));
	// Array containing a bit for each possible colour. bit is set to 1 if colour has been found
	coloursFoundBool = (uint32_t*)calloc(524288, sizeof(uint32_t));
	params = (struct threadParams*)calloc(threads, sizeof(struct threadParams));
	currentDyes = (uint8_t*)calloc(8, sizeof(uint8_t));
	coloursFoundByThreads = (struct colourMix*)calloc(COLOUROPTIONSCOUNT * BASECOLOURSPERTHREAD * threads, sizeof(struct colourMix));
	tid = malloc(sizeof(pthread_t) * threads);

	char filename[100] = { 0 };

	// read all possible dye combinations to test (8 combinations as there are 8 free slots in the crafting gui)
	fp = fopen("colourOptions_depth_8.log", "r");
	i = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		colour = atol(line);
		colourOptionsInt[i] = colour;
		getDyesForInt(colour, &colourOptions[i * COLOURCOUNT]);
		i++;
	}
	fclose(fp);

	// read progress within layer
	sprintf(filename, "progress_%s", version);
    fpProgress = fopen(filename, "a+");
	if(fpProgress != NULL && (read = getline(&line, &len, fpProgress)) != -1) {
		progress = atoi(line);
	}
	fclose(fpProgress);

	// read current layer
    sprintf(filename, "layer_progress_%s", version);
    fpLayer = fopen(filename, "a+");
	if(fpLayer != NULL && (read = getline(&line, &len, fpLayer)) != -1) {
		layerCount = atoi(line);
	}
	fclose(fpLayer);

    nextArmorBaseCount = 0;

	// read found colours for all finished crafting layers, to set bit in coloursFoundBool-Array
    for(i = 0; i < layerCount; i++) {
        sprintf(filename, "colourMap_Layer_%li_Depth_8_%s.log", i, version);
        fp = fopen(filename, "r");
	
        while ((read = getline(&line, &len, fp)) != -1) {
            colour = atoi(line);
            if(colour == 0) continue;
            coloursFoundBool[colour >> 5] = coloursFoundBool[colour >> 5] | (1 << (colour & 0x1F));

            if(i == layerCount - 1) nextArmorBaseCount++;
        }
        fclose(fp);
    }

	// if the programm starts from layer 0 baseColour -1 (=no colour) is inserted in the list of baseColours to test
    if(layerCount == 0) {
        newArmorBase = (uint32_t*)calloc(1, sizeof(uint32_t));
        nextArmorBaseCount = 1;
		// if you want to calculate the possibilites based on a specific base colour in can be inserted here
        newArmorBase[0] = -1;
    }
	// if there are already finished layers the last finished layer is used for the list of baseColours to test
    else {
        newArmorBase = (uint32_t*)calloc(nextArmorBaseCount, sizeof(uint32_t));

        sprintf(filename, "colourMap_Layer_%u_Depth_8_%s.log", layerCount - 1, version);
        fp = fopen(filename, "r");

        i = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
            colour = atoi(line);
            if(colour == 0) continue;
            newArmorBase[i] = colour;
            i++;
        }
        fclose(fp);
    }

	for (i = 0; i < threads; i++) {
		params[i].coloursFoundBool = coloursFoundBool;
		params[i].colourOptionsInt = colourOptionsInt;
		params[i].colourOptions = colourOptions;
		params[i].coloursFound = &coloursFoundByThreads[i * COLOUROPTIONSCOUNT * BASECOLOURSPERTHREAD];
	}

	// if the progress within the current layer is > 0 the bits for the already found colours are set in the coloursFoundBool-Array
	if(progress > 0) {
		sprintf(filename, "colourMap_Layer_%u_Depth_8_%s.log", layerCount, version);
		fp = fopen(filename, "r");

		while ((read = getline(&line, &len, fp)) != -1) {
            colour = atoi(line);
            if(colour == 0) continue;
            coloursFoundBool[colour >> 5] = coloursFoundBool[colour >> 5] | (1 << (colour & 0x1F));
        }

        fclose(fp);
	}

    receivedSignal = 0;
    signal(SIGINT, exit_after_finishing);

    printf("Starting in Layer %i from %i\n", layerCount, progress);

	// loop for all layers until there are no newly found colours
	while(nextArmorBaseCount > 0) {
		newArmorBaseCount = nextArmorBaseCount;
		nextArmorBaseCount = 0;
		
		sprintf(filename, "colourMap_Layer_%u_Depth_8_%s.log", layerCount, version);
		fp = fopen(filename, "a");

		// main loop to test all colours for the current layer
		for (iB = progress; iB < newArmorBaseCount;) {
            if(receivedSignal) {
                break;
            }

			// create all threads. each thread checks BASECOLOURSPERTHREAD basecolours
			for (i = 0; i < threads; i++) {
				params[i].coloursFoundCount = 0;
				params[i].colourBase = &newArmorBase[iB];
				params[i].colourBaseOffset = iB;
				params[i].colourBaseCount = newArmorBaseCount;
				iB += BASECOLOURSPERTHREAD;

				returnCode = pthread_create(&tid[i], NULL, test_dyecombs, (void*)&params[i]);
				if (returnCode != 0)
				{
					printf("Failed to create threads!\n");
					return 0;
				}
			}

			stop = time(NULL);
			char date_str[100] = { 0 };
			t = localtime(&stop);
			strftime(date_str, sizeof(date_str) - 1, "[%d.%m.%Y - %H:%M:%S]", t);

			printf("%s Threads created\n", date_str);

			// collect all threads, after they finished testing the given basecolours. write all found colours into logfile
			for (i = 0; i < threads; i++)
			{
				returnCode = pthread_join(tid[i], NULL);

				for (i2 = 0; i2 < params[i].coloursFoundCount; i2++) {
					fprintf(fp, "%i#%i#%lu\n", params[i].coloursFound[i2].colour, params[i].coloursFound[i2].armorBaseColour, params[i].coloursFound[i2].newColours);
					nextArmorBaseCount++;
				}
			}

			// log progress
			sprintf(filename, "progress_%s", version);
            fpProgress = fopen(filename, "w");
            fprintf(fpProgress, "%lu", iB);
            fclose(fpProgress);
		}

		progress = 0;

		fclose(fp);

		free(newArmorBase);

        if(receivedSignal) {
            break;
        }
		
		// read all newly found colours from the current layer to use as basecolours for the next layer
		sprintf(filename, "colourMap_Layer_%u_Depth_8_%s.log", layerCount, version);
        fp = fopen(filename, "r");

        nextArmorBaseCount = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
            colour = atoi(line);
            if(colour == 0) continue;
            nextArmorBaseCount++;
        }
        fclose(fp);

        newArmorBase = (uint32_t*)calloc(nextArmorBaseCount, sizeof(uint32_t));

        fp = fopen(filename, "r");

        i = 0;
        while ((read = getline(&line, &len, fp)) != -1) {
            colour = atoi(line);
            if(colour == 0) continue;
            newArmorBase[i] = colour;
            i++;
        }
        fclose(fp);

		printf("Finished Layer %u, found %u new colours!\n", layerCount, nextArmorBaseCount);

		layerCount++;

		sprintf(filename, "layer_progress_%s", version);
        fpLayer = fopen(filename, "w");
        fprintf(fpLayer, "%i", layerCount);
        fclose(fpLayer);
	}

	if(!receivedSignal) {
	    printf("Finished!\n");
    }
    else {
        printf("Program stopped!\n");
    }

	return 0;
}

// function executed by each thread
void* test_dyecombs(void* args) {
	struct threadParams* argsPtr;
	uint32_t i, iB;
	argsPtr = (struct threadParams*)args;
	uint8_t* currentDyes;
	uint32_t colour;
	struct colourMix colourmix;

	argsPtr->coloursFoundCount = 0;
	
	// calculates the resulting colour for each basecolour and for each addable dyecombination
	for (iB = 0; iB < BASECOLOURSPERTHREAD; iB++) {
		if((argsPtr->colourBaseOffset + iB) >= argsPtr->colourBaseCount) {
			pthread_exit((void*)NULL);
		}
		for (i = 0; i < COLOUROPTIONSCOUNT; i++) {
			currentDyes = &argsPtr->colourOptions[i * COLOURCOUNT];
		
			colour = colourForCombination(currentDyes, argsPtr->colourBase[iB]);

			// if the colour wasn't already found it is globaly marked as found and appended to the list of newly found colours
			if ((argsPtr->coloursFoundBool[colour >> 5] & (1 << (colour & 0x1F))) == 0) {
				argsPtr->coloursFoundBool[colour >> 5] = argsPtr->coloursFoundBool[colour >> 5] | (1 << (colour & 0x1F));

				colourmix.armorBaseColour = argsPtr->colourBase[iB];
				colourmix.colour = colour;
				colourmix.newColours = argsPtr->colourOptionsInt[i];

				argsPtr->coloursFound[argsPtr->coloursFoundCount] = colourmix;
				argsPtr->coloursFoundCount++;
			}
		}
	}

	pthread_exit((void*)NULL);
}
