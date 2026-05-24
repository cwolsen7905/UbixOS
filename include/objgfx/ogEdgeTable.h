#ifndef OGEDGETABLE_H
#define OGEDGETABLE_H

#include "ogTypes.h"
#include "objgfx.h" 

struct ogEdgeState 
{
	ogEdgeState* nextEdge;
	int32        x;
	int32        startY;
	int32        wholePixelXMove;
	int32        xDirection;
	int32        errorTerm;
	int32        errorTermAdjUp;
	int32        errorTermAdjDown;
	int32        count;
	ogRGBA8      colour;
	int32        rStepY;
	int32        gStepY;
	int32        bStepY;
	int32        aStepY;
	int32        rIncY;
	int32        gIncY;
	int32        bIncY;
	int32        aIncY;
};

class ogEdgeTable 
{
public:
	ogEdgeState * globalEdges;
	ogEdgeState * activeEdges;
	ogEdgeTable(void) { globalEdges = activeEdges = nullptr; return; }
	void AdvanceAET(void);
	void BuildGET(uInt32 numPoints, ogPoint2d * polyPoints);
	void BuildGET_G(uInt32 numPoints, ogPoint2d * polyPoints, ogRGBA8 * colours);
	void MoveXSortedToAET(int32 yToMove);
	void ScanOutAET(ogSurface & destObject, int32 yToScan, uInt32 colour);
	void ScanOutAET_G(ogSurface & destObject, int32 yToScan);
	void XSortAET(void);
	~ogEdgeTable(void);
}; // ogEdgeTable

#endif