/****************************************************************************
*                                                                           *
*  OpenNI 1.x Alpha                                                         *
*  Copyright (C) 2011 PrimeSense Ltd.                                       *
*                                                                           *
*  This file is part of OpenNI.                                             *
*                                                                           *
*  OpenNI is free software: you can redistribute it and/or modify           *
*  it under the terms of the GNU Lesser General Public License as published *
*  by the Free Software Foundation, either version 3 of the License, or     *
*  (at your option) any later version.                                      *
*                                                                           *
*  OpenNI is distributed in the hope that it will be useful,                *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the             *
*  GNU Lesser General Public License for more details.                      *
*                                                                           *
*  You should have received a copy of the GNU Lesser General Public License *
*  along with OpenNI. If not, see <http://www.gnu.org/licenses/>.           *
*                                                                           *
****************************************************************************/
//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------
#include <XnOpenNI.h>
#include <XnLog.h>
#include <XnCppWrapper.h>
#include <XnFPSCalculator.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>

using namespace std;


//---------------------------------------------------------------------------
// Module Defines
//---------------------------------------------------------------------------
#define CUBICUT 0
#define PRINTF_DEBUG 0
#define FRAME_DIFF 1
#define CSV_RECORD_LONG_PERIOD 1
#define CSV_RECORD 0

//---------------------------------------------------------------------------
// Defines
//---------------------------------------------------------------------------
#define SAMPLE_XML_PATH "../../Config/SamplesConfig.xml"
#define SAMPLE_XML_PATH_LOCAL "SamplesConfig.xml"
#define FRAME_NUM_MAX 35
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 480
#define CUT_X (FRAME_WIDTH / 2)
#define CUT_Y (FRAME_HEIGHT / 2)
#define CSV_FILE "large_movement.csv"
#define DEPTH_THRESHOLD 100

//---------------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------------
#define CHECK_RC(rc, what)											\
	if (rc != XN_STATUS_OK)											\
	{																\
		printf("%s failed: %s\n", what, xnGetStatusString(rc));		\
		return rc;													\
	}

//---------------------------------------------------------------------------
// Code
//---------------------------------------------------------------------------

using namespace xn;

XnBool fileExists(const char *fn)
{
	XnBool exists;
	xnOSDoesFileExist(fn, &exists);
	return exists;
}

int main()
{
	XnStatus nRetVal = XN_STATUS_OK;

	Context context;
	ScriptNode scriptNode;
	EnumerationErrors errors;

	const char *fn = NULL;
	if	(fileExists(SAMPLE_XML_PATH)) fn = SAMPLE_XML_PATH;
	else if (fileExists(SAMPLE_XML_PATH_LOCAL)) fn = SAMPLE_XML_PATH_LOCAL;
	else {
		printf("Could not find '%s' nor '%s'. Aborting.\n" , SAMPLE_XML_PATH, SAMPLE_XML_PATH_LOCAL);
		return XN_STATUS_ERROR;
	}
	printf("Reading config from: '%s'\n", fn);
	nRetVal = context.InitFromXmlFile(fn, scriptNode, &errors);

	if (nRetVal == XN_STATUS_NO_NODE_PRESENT)
	{
		XnChar strError[1024];
		errors.ToString(strError, 1024);
		printf("%s\n", strError);
		return (nRetVal);
	}
	else if (nRetVal != XN_STATUS_OK)
	{
		printf("Open failed: %s\n", xnGetStatusString(nRetVal));
		return (nRetVal);
	}

	DepthGenerator depth;
	nRetVal = context.FindExistingNode(XN_NODE_TYPE_DEPTH, depth);
	CHECK_RC(nRetVal, "Find depth generator");

	XnFPSData xnFPS;
	nRetVal = xnFPSInit(&xnFPS, 180);
	CHECK_RC(nRetVal, "FPS Init");

	DepthMetaData depthMD;
	
	int i, j, diff;
	int frame_id;

#if CUBICUT
	int x_time_rectangle[FRAME_WIDTH][FRAME_NUM_MAX];
	int y_time_rectangle[FRAME_HEIGHT][FRAME_NUM_MAX];
#endif

#if FRAME_DIFF
	int per_frame_depth[FRAME_WIDTH][FRAME_HEIGHT];
#endif

#if CSV_RECORD || CSV_RECORD_LONG_PERIOD
	ofstream ofs(CSV_FILE);
#endif


	while (!xnOSWasKeyboardHit())
	{
		nRetVal = context.WaitOneUpdateAll(depth);
		if (nRetVal != XN_STATUS_OK)
		{
			printf("UpdateData failed: %s\n", xnGetStatusString(nRetVal));
			continue;
		}

		xnFPSMarkFrame(&xnFPS);
	
		depth.GetMetaData(depthMD);
		
		frame_id = depthMD.FrameID() - 1;

#if CUBICUT
		/* CubiCut Start */
		
		for (i=0; i<FRAME_WIDTH; i++) {
			// first frame
			if (!frame_id) {
				x_time_rectangle[i][frame_id] = (int)depthMD(i, CUT_Y);
			} else {
				diff = abs((int)depthMD(i, CUT_Y) - x_time_rectangle[i][0]);
				if (diff > 100) {
					x_time_rectangle[i][frame_id] = diff;
				} else {
					x_time_rectangle[i][frame_id] = 0;
				}
			}
		}

		for (j=0; j<FRAME_HEIGHT; j++) {
			// first frame
			if (!frame_id) {
				y_time_rectangle[j][frame_id] = (int)depthMD(i, CUT_X);
			} else {
				diff = abs((int)depthMD(j, CUT_X) - y_time_rectangle[j][0]);
				if (diff > 100) {
					y_time_rectangle[j][frame_id] = diff;
				} else {
					y_time_rectangle[j][frame_id] = 0;
				}
			}
		}
		/* CubiCut End */
#endif

#if CSV_RECORD
		ofs << "frame" << frame_id << endl;
		for (j=0; j<FRAME_HEIGHT; j++) {
			for (i=0; i<FRAME_WIDTH; i++) {
				// first frame
				if (!frame_id) {
					per_frame_depth[i][j] = (int)depthMD(i, j); 
					ofs << (int)depthMD(i, j);
				} else {
					// frame difference
					diff = abs((int)depthMD(i, j) - per_frame_depth[i][j]);
					if (diff < DEPTH_THRESHOLD) {
						diff = 0;
					}
					ofs << diff;
				}
				
				// comma or next row
				if (i == (FRAME_WIDTH - 1)) {
					ofs << endl;
				} else {
					ofs << ",";
				}
			}
		}
		ofs << endl;
#endif
#if CSV_RECORD_LONG_PERIOD
		if (-1 < frame_id && frame_id < 5) {
			continue;
		} 
		for (j=0; j<FRAME_HEIGHT; j++) {
			for (i=0; i<FRAME_WIDTH; i++) {
				// first frame
				if (frame_id == 5) {
					per_frame_depth[i][j] = (int)depthMD(i, j); 
					ofs << (int)depthMD(i, j);
				} else {
					// frame difference
					diff = abs((int)depthMD(i, j) - per_frame_depth[i][j]);
					if (diff < DEPTH_THRESHOLD) {
						diff = 0;
					}
					ofs << diff;
				}
				
				// comma or next row
				ofs << ",";
			}
		}
		ofs << endl;
#endif
		if (depthMD.FrameID() == FRAME_NUM_MAX) break;
	}

#if PRINTF_DEBUG
	/* Printf Debug Start */
	int x, y, t;
	
	printf("x_time_rectangle\n");
	printf("[ ");
	for (x=0; x<FRAME_WIDTH; x++) {
		for (t=0; t<FRAME_NUM_MAX; t++) {
			printf("%d ", x_time_rectangle[x][t]);
		}
		printf("\n");
	}
	printf("]\n\n");
	
	
	printf("y_time_rectangle\n");
	printf("[ ");
	for (y=0; y<FRAME_HEIGHT; y++) {
		for (t=0; t<FRAME_NUM_MAX; t++) {
			printf("%d ", y_time_rectangle[y][t]);
		}
		printf("\n");
	}
	printf("]\n\n");
	/* Printf Debug End */
#endif

	depth.Release();
	scriptNode.Release();
	context.Release();

	return 0;
}
