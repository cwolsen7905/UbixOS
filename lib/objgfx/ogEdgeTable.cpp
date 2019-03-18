#include <math.h>
#include "ogEdgeTable.h"

void ogEdgeTable::AdvanceAET(void) 
{
	ogEdgeState *  currentEdge;
	ogEdgeState ** currentEdgePtr;

	currentEdgePtr = &activeEdges;
	currentEdge = activeEdges;
	while (currentEdge!=NULL) {
		--currentEdge->count;
		if (currentEdge->count == 0) 
		{
			// this edge is finished, so remove it from the AET
			*currentEdgePtr = currentEdge->nextEdge;
			// I'm thinking I should dispose currentEdge here!?
		} 
		else 
		{
			// advance the edge's x coord by minimum move
			currentEdge->x += currentEdge->wholePixelXMove;
			// determine whether it's time for X to advance one extra
			currentEdge->errorTerm += currentEdge->errorTermAdjUp;
			if (currentEdge->errorTerm > 0)
			{
				currentEdge->x += currentEdge->xDirection;
				currentEdge->errorTerm -= currentEdge->errorTermAdjDown;
			} // if
			currentEdgePtr = &currentEdge->nextEdge;
		} // else
		currentEdge = *currentEdgePtr;
	} // while
} // void ogEdgeTable::AdvanceAET()

void ogEdgeTable::BuildGET(uInt32 numPoints, ogPoint2d * polyPoints) 
{
	int32 i, x1, y1, x2, y2, deltaX, deltaY, width, tmp;
	ogEdgeState *  newEdgePtr;
	ogEdgeState *  followingEdge;
	ogEdgeState ** followingEdgeLink;

	/* 
	* Creates a GET in the buffer pointed to by NextFreeEdgeStruc from
	* the vertex list. Edge endpoints are flipped, if necessary, to
	* guarantee all edges go top to bottom. The GET is sorted primarily
	* by ascending Y start coordinate, and secondarily by ascending X
	* start coordinate within edges with common Y coordinates }
	*/

	// Scan through the vertex list and put all non-0-height edges into
	// the GET, sorted by increasing Y start coordinate}

	for (i = 0; i < (int32)numPoints; i++) 
	{
		// calculate the edge height and width
		x1 = polyPoints[i].x;
		y1 = polyPoints[i].y;

		if (i == 0) 
		{
			// wrap back around to the end of the list
			x2 = polyPoints[numPoints-1].x;
			y2 = polyPoints[numPoints-1].y;
		} 
		else 
		{
			x2 = polyPoints[i-1].x;
			y2 = polyPoints[i-1].y;
		} // else i!=0

		if (y1 > y2) 
		{
			tmp = x1;
			x1  = x2;
			x2  = tmp;

			tmp = y1;
			y1  = y2;
			y2  = tmp;
		} // if y1>y2

		// skip if this can't ever be an active edge (has 0 height)
		deltaY = y2-y1;
		if (deltaY != 0) 
		{
			newEdgePtr = new ogEdgeState;
			newEdgePtr->xDirection = ((deltaX = x2-x1) > 0) ? 1 : -1;
			width = fabs(deltaX);
			newEdgePtr->x = x1;
			newEdgePtr->startY = y1;
			newEdgePtr->count = newEdgePtr->errorTermAdjDown = deltaY;
			newEdgePtr->errorTerm = (deltaX >= 0) ? 0 : 1-deltaY;

			if (deltaY >= width) 
			{
				newEdgePtr->wholePixelXMove = 0;
				newEdgePtr->errorTermAdjUp = width;
			} 
			else 
			{
				newEdgePtr->wholePixelXMove = (width / deltaY) * newEdgePtr->xDirection;
				newEdgePtr->errorTermAdjUp = width % deltaY;
			} // else

			followingEdgeLink = &globalEdges;
			while (true) {
				followingEdge = *followingEdgeLink;
				if ((followingEdge == NULL) ||
					(followingEdge->startY > y1) ||
					((followingEdge->startY == y1) &&
					(followingEdge->x>=x1))) 
				{
					newEdgePtr->nextEdge = followingEdge;
					*followingEdgeLink = newEdgePtr;
					break;
				} // if
				followingEdgeLink = &followingEdge->nextEdge;
			} // while
		} // if deltaY!=0
	} // for
} // void ogEdgeTable::BuildGET()

void ogEdgeTable::BuildGET_G(uInt32 numPoints, ogPoint2d * polyPoints, ogRGBA8 * colours) 
{

	int32 i, x1, y1, x2, y2, deltaX, deltaY, width, tmp;
	ogEdgeState *  newEdgePtr;
	ogEdgeState *  followingEdge;
	ogEdgeState ** followingEdgeLink;
	ogRGBA8 c1, c2, cTmp;

	/* 
	* Creates a GET in the buffer pointed to by NextFreeEdgeStruc from
	* the vertex list. Edge endpoints are flipped, if necessary, to
	* guarantee all edges go top to bottom. The GET is sorted primarily
	* by ascending Y start coordinate, and secondarily by ascending X
	* start coordinate within edges with common Y coordinates }
	*/

	// Scan through the vertex list and put all non-0-height edges into
	// the GET, sorted by increasing Y start coordinate}

	for (i = 0; i < (int32)numPoints; i++) 
	{
		// calculate the edge height and width
		x1 = polyPoints[i].x;
		y1 = polyPoints[i].y;
		c1 = colours[i];

		if (0 == i) 
		{
			// wrap back around to the end of the list
			x2 = polyPoints[numPoints-1].x;
			y2 = polyPoints[numPoints-1].y;
			c2 = colours[numPoints-1];
		} 
		else 
		{
			x2 = polyPoints[i-1].x;
			y2 = polyPoints[i-1].y;
			c2 = colours[i-1];
		} // else i!=0

		if (y1 > y2) 
		{
			tmp = x1;
			x1  = x2;
			x2  = tmp;

			tmp = y1;
			y1  = y2;
			y2  = tmp;

			cTmp = c1;
			c1 = c2;
			c2 = cTmp;
		} // if y1>y2

		// skip if this can't ever be an active edge (has 0 height)
		deltaY = y2-y1;
		if (deltaY != 0) 
		{
			newEdgePtr = new ogEdgeState;
			newEdgePtr->colour = c1;
			newEdgePtr->xDirection = ((deltaX = x2-x1) > 0) ? 1 : -1;

			newEdgePtr -> rStepY = ((c2.red - c1.red +1) << 16) / deltaY;
			newEdgePtr -> gStepY = ((c2.green - c1.green +1) << 16) / deltaY;
			newEdgePtr -> bStepY = ((c2.blue - c1.blue +1) << 16) / deltaY;
			newEdgePtr -> aStepY = ((c2.alpha - c1.alpha +1) << 16) / deltaY;

			newEdgePtr -> rIncY = newEdgePtr -> gIncY = 0;
			newEdgePtr -> bIncY = newEdgePtr -> aIncY = 0;

			width = fabs(deltaX);
			newEdgePtr->x = x1;
			newEdgePtr->startY = y1;
			newEdgePtr->count = newEdgePtr->errorTermAdjDown = deltaY;
			newEdgePtr->errorTerm = (deltaX >= 0) ? 0 : 1-deltaY;

			if (deltaY >= width) 
			{
				newEdgePtr->wholePixelXMove = 0;
				newEdgePtr->errorTermAdjUp = width;
			} 
			else 
			{
				newEdgePtr->wholePixelXMove = (width / deltaY) * newEdgePtr->xDirection;
				newEdgePtr->errorTermAdjUp = width % deltaY;
			} // else

			followingEdgeLink = &globalEdges;
			while (true) {
				followingEdge = *followingEdgeLink;
				if ((followingEdge == NULL) ||
					(followingEdge->startY > y1) ||
					((followingEdge->startY == y1) &&
					(followingEdge->x >= x1))) 
				{
					newEdgePtr->nextEdge = followingEdge;
					*followingEdgeLink = newEdgePtr;
					break;
				} // if
				followingEdgeLink = &followingEdge->nextEdge;
			} // while
		} // if deltaY!=0
	} // for
	return;
} // ogEdgeTable::BuildGET_G

void ogEdgeTable::MoveXSortedToAET(int32 yToMove) 
{
	ogEdgeState *  AETEdge;
	ogEdgeState *  tempEdge;
	ogEdgeState ** AETEdgePtr;
	int32          currentX;

	/* The GET is Y sorted. Any edges that start at the d%%esired Y
	* coordinate will be first in the GET, so we'll move edges from
	* the GET to AET until the first edge left in the GET is no
	* longer at the d%%esired Y coordinate. Also, the GET is X sorted
	* within each Y cordinate, so each successive edge we add to the
	* AET is guaranteed to belong later in the AET than the one just
	* added.
	*/
	AETEdgePtr = &activeEdges;
	while ((globalEdges != NULL) && (globalEdges->startY == yToMove)) 
	{
		currentX = globalEdges->x;
		// link the new edge into the AET so that the AET is still
		// sorted by X coordinate
		while (true) {
			AETEdge = *AETEdgePtr;
			if ((AETEdge == NULL) || (AETEdge->x >= currentX)) 
			{
				tempEdge = globalEdges->nextEdge;
				*AETEdgePtr = globalEdges;
				globalEdges->nextEdge = AETEdge;
				AETEdgePtr = &globalEdges->nextEdge;
				globalEdges = tempEdge;
				break;
			} else AETEdgePtr = &AETEdge->nextEdge;
		} // while true
	} // while globalEdges!=NULL and globalEdges->startY==yToMove
} // void  ogEdgeTable::MoveXSortedToAET()

void ogEdgeTable::ScanOutAET(ogSurface & destObject, int32 yToScan, uInt32 colour) 
{
	ogEdgeState * currentEdge;
	int32 leftX;

	/*  Scan through the AET, drawing line segments as each pair of edge
	*  crossings is encountered. The nearest pixel on or to the right
	*  of the left edges is drawn, and the nearest pixel to the left
	*  of but not on right edges is drawn
	*/
	currentEdge = activeEdges;

	while (currentEdge != NULL) {
		leftX = currentEdge->x;
		currentEdge = currentEdge->nextEdge;

		if (currentEdge != NULL) 
		{
			if (currentEdge->x > leftX)
				destObject.ogHLine(leftX, currentEdge->x-1, yToScan, colour);
			currentEdge = currentEdge->nextEdge;
		} // if currentEdge != NULL
	} // while

	return;
} // void ogEdgeTable::ScanOutAET()

void ogEdgeTable::ScanOutAET_G(ogSurface & destObject, int32 yToScan)
{
	ogEdgeState * currentEdge;
	int32 leftX, count;
	int32 rStepX, gStepX, bStepX, aStepX;
	int32 rIncX, gIncX, bIncX, aIncX;
	int32 lR, lG, lB, lA;
	int32 rR, rG, rB, rA;
	int32 dR, dG, dB, dA;
	int32 dist;

	/*  Scan through the AET, drawing line segments as each pair of edge
	*  crossings is encountered. The nearest pixel on or to the right
	*  of the left edges is drawn, and the nearest pixel to the left
	*  of but not on right edges is drawn
	*/
	currentEdge = activeEdges;

	while (currentEdge != NULL)
	{
		leftX = currentEdge->x;

		lR = currentEdge->colour.red;
		lG = currentEdge->colour.green;
		lB = currentEdge->colour.blue;
		lA = currentEdge->colour.alpha;

		lR += currentEdge->rIncY >> 16;
		lG += currentEdge->gIncY >> 16;
		lB += currentEdge->bIncY >> 16;
		lA += currentEdge->aIncY >> 16;

		currentEdge->rIncY += currentEdge->rStepY;
		currentEdge->gIncY += currentEdge->gStepY;
		currentEdge->bIncY += currentEdge->bStepY;
		currentEdge->aIncY += currentEdge->aStepY;


		currentEdge = currentEdge->nextEdge;

		if (currentEdge != NULL) 
		{
			if (leftX != currentEdge->x) 
			{
				rR = currentEdge->colour.red;
				rG = currentEdge->colour.green;
				rB = currentEdge->colour.blue;
				rA = currentEdge->colour.alpha;

				rR += currentEdge->rIncY >> 16;
				rG += currentEdge->gIncY >> 16;
				rB += currentEdge->bIncY >> 16;
				rA += currentEdge->aIncY >> 16;

				currentEdge->rIncY += currentEdge->rStepY;
				currentEdge->gIncY += currentEdge->gStepY;
				currentEdge->bIncY += currentEdge->bStepY;
				currentEdge->aIncY += currentEdge->aStepY;

				dR = rR - lR;
				dG = rG - lG;
				dB = rB - lB;
				dA = rA - lA;

				dist = currentEdge->x - leftX;

				rStepX = (dR << 16) / dist;
				gStepX = (dG << 16) / dist;
				bStepX = (dB << 16) / dist;
				aStepX = (dA << 16) / dist;
				rIncX = gIncX = bIncX = aIncX = 0;

				for (count = leftX; count < currentEdge->x; count++) 
				{
					destObject.ogSetPixel(count, yToScan,
						static_cast<uInt8>(lR + (rIncX >> 16)),
						static_cast<uInt8>(lG + (gIncX >> 16)),
						static_cast<uInt8>(lB + (bIncX >> 16)),
						static_cast<uInt8>(lA + (aIncX >> 16)) );
					rIncX += rStepX;
					gIncX += gStepX;
					bIncX += bStepX;
					aIncX += aStepX;
				} // for
			}
			currentEdge = currentEdge->nextEdge;
		} // if currentEdge != NULL
	} // while

} // void ogEdgeTable::ScanOutAET_G()

void ogEdgeTable::XSortAET(void) 
{
	ogEdgeState *  currentEdge;
	ogEdgeState *  tempEdge;
	ogEdgeState ** currentEdgePtr;
	bool swapOccurred;

	if (activeEdges == NULL) return;

	do {
		swapOccurred = false;
		currentEdgePtr = &activeEdges;
		currentEdge = activeEdges;
		while (currentEdge->nextEdge != NULL) 
		{
			if (currentEdge->x > currentEdge->nextEdge->x) 
			{
				// the second edge has a lower x than the first
				// swap them in the AET
				tempEdge = currentEdge->nextEdge->nextEdge;
				*currentEdgePtr = currentEdge->nextEdge;
				currentEdge->nextEdge->nextEdge = currentEdge;
				currentEdge->nextEdge = tempEdge;
				swapOccurred = true;
			} // if
			currentEdgePtr = &((*currentEdgePtr)->nextEdge);
			currentEdge = *currentEdgePtr;
		} // while
	} while (swapOccurred);
} // void ogEdgeTable::XSortAET()

ogEdgeTable::~ogEdgeTable(void) 
{
	ogEdgeState * edge;
	ogEdgeState * tmpEdge;
	tmpEdge = globalEdges;
	// first walk the global edges and delete any non-null nodes
	while (tmpEdge != NULL) 
	{
		edge = tmpEdge;
		tmpEdge = edge->nextEdge;
		delete edge;
	} // while
	tmpEdge = activeEdges;
	// next walk the activeEdges and delete any non-null nodes.  Note that this should
	// always be null
	while (tmpEdge != NULL) 
	{
		edge = tmpEdge;
		tmpEdge = edge->nextEdge;
		delete edge;
	} // while
	return;
} // ogEdgeTable::~ogEdgeTable()
