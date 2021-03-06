/* praat_MDS_init.c
 *
 * Copyright (C) 1992-2010 David Weenink
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 djmw 20020408 GPL
 djmw 20020408 Added MDS-tutorial
 djmw 20020603 Changes due to TableOfReal dynamic menu changes.
 djmw 20040415 Forms texts.
 djmw 20040513 More forms text changes
 djmw 20041027 Orhogonal transform parameter for Configurations_to_Procrustes
 djmw 20050406 classProcrustus -> classProcrustes.
 djmw 20050426 Removed "Procrustus.h"
 djmw 20050630 Better name of Procrustes object after Configurations_to_Procrustes.
 djmw 20061218 Introduction of Melder_information<12...9>
 djmw 20070902 Melder_new<1...>
 djmw 20071011 REQUIRE requires L"ui/editors/AmplitudeTierEditor.h".
 djmw 20090818 Thing_recognizeClassesByName: added classAffineTransform, classScalarProduct, classWeight
*/

#include "ui/praat.h"

#include "dwtools/Configuration_and_Procrustes.h"
#include "dwtools/Configuration_AffineTransform.h"
#include "dwtools/Confusion.h"
#include "dwtools/MDS.h"
#include "dwtools/TableOfReal_extensions.h"
#include "num/NUM2.h"
#include "ui/Formula.h"

#include <math.h>

int TableOfReal_formula (I, const wchar_t *expression, Interpreter *interpreter, thou);

void praat_TableOfReal_init (void *klas);

void SSCPs_drawConcentrationEllipses (SSCPs me, Graphics g, double scale,
	int confidence, wchar_t *label, long d1, long d2, double xmin, double xmax,
	double ymin, double ymax, int fontSize, int garnish);
void Proximity_Distance_drawScatterDiagram (I, Distance thee, Graphics g, 
	double xmin, double xmax, double ymin, double ymax, double size_mm, 
	const wchar_t *mark, int garnish);
void Dissimilarity_Configuration_Weight_drawISplineRegression 
	(Dissimilarity d, Configuration c, Weight w, Graphics g,
	long numberOfInternalKnots, long order, double xmin, double xmax, 
	double ymin, double ymax, double size_mm, const wchar_t *mark, int garnish);

static wchar_t *QUERY_BUTTON   = L"Query -";
static wchar_t *DRAW_BUTTON    = L"Draw -";
static wchar_t *ANALYSE_BUTTON = L"Analyse -";
static wchar_t *CONFIGURATION_BUTTON = L"To Configuration -";
extern "C" void praat_TableOfReal_init2  (void *klas);

/* Tests */

/*
	Sort row 1 ascending and store in row 3
	Sort row 1 and move row 2 along and store in rows 4 and 5 respectively
	Make an index for row 1 and store in row 6
*/
static int TabelOfReal_testSorting (I, long rowtoindex)
{
	iam (TableOfReal);
	long i, nr = my numberOfRows, nc = my numberOfColumns;
	long *index = NUMlvector (1, nc);

	if (index == NULL) return 0;
	if (nr < 6) return Melder_error1 (L"TabelOfReal_sort2: we want at least 6 rows!!");
	if (rowtoindex < 1 || rowtoindex > 2)
		return Melder_error1 (L"TabelOfReal_sort2: rowtoindex <= 2");

	/* Copy 1->3 and sort 3 inplace */
	NUMdvector_copyElements (my data[1], my data[3], 1, nc);
	NUMsort_d (nc, my data[3]);

	/* Copy 1->4 and 2->5, sort 4+5 in parallel */
	NUMdvector_copyElements (my data[1], my data[4], 1, nc);
	NUMdvector_copyElements (my data[2], my data[5], 1, nc);
	NUMsort2_dd (nc, my data[4], my data[5]);

	/* make index */
	NUMindexx (my data[rowtoindex], nc, index);
	for (i = 1; i <= nc; i++) my data[6][i] = index[i];
	NUMlvector_free (index, 1);
	return 1;
}

FORM (TabelOfReal_testSorting, L"TabelOfReal: Sort and index", L"ui/editors/AmplitudeTierEditor.h")
	NATURAL (L"Row to index", L"1")
	OK
DO
	if (! TabelOfReal_testSorting (ONLY_OBJECT, GET_INTEGER (L"Row to index"))) return 0;
END

/************************* examples ***************************************/

FORM (Dissimilarity_createLetterRExample, L"Create letter R example", L"Create letter R example...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"For the monotone transformation on the distances")
	REAL (L"Noise range", L"32.5")
	OK
DO
	(void) praat_new1 (Dissimilarity_createLetterRExample
		(GET_REAL (L"Noise range")), NULL);
END

FORM (INDSCAL_createCarrollWishExample,
	L"Create INDSCAL Carroll & Wish example...",
	L"Create INDSCAL Carroll & Wish example...")
	REAL (L"Noise standard deviation", L"0.0")
	OK
DO
	(void) praat_new1 (INDSCAL_createCarrollWishExample
		(GET_REAL (L"Noise standard deviation")), NULL);
END

FORM (Configuration_create, L"Create Configuration", L"Create Configuration...")
	WORD (L"Name", L"uniform")
	NATURAL (L"Number of points", L"10")
	NATURAL (L"Number of dimensions", L"2")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Formula:")
	TEXTFIELD (L"formula", L"randomUniform(-1.5, 1.5)")
	OK
DO
	Configuration c = Configuration_create (GET_INTEGER (L"Number of points"),
		GET_INTEGER (L"Number of dimensions"));
	if (c == NULL || ! praat_new1 (c, GET_STRING (L"Name"))) return 0;
	if (! TableOfReal_formula (c, GET_STRING (L"formula"), interpreter, NULL)) return 0;
END

void drawSplines (Graphics g, double low, double high, double ymin, double ymax,
	int type, long order, wchar_t *interiorKnots, int garnish)
{
	long i, j, k = order, numberOfKnots, numberOfInteriorKnots = 0;
	long nSplines, n = 1000;
	double knot[101]; double y[1001];
	wchar_t *start, *end;
	
	if (type == MDS_ISPLINE) k++;
	for (i = 1; i <= k; i++)
	{
		knot[i] = low;
	}
	numberOfKnots = k;
	
	start = interiorKnots;
	while (*start)
	{
		double value = wcstod (start, &end);
		start = end;
		if (value < low || value > high)
		{
			Melder_warning5 (L"drawSplines: knots must be in interval (", Melder_double (low), L", ", Melder_double (high), L")");
			return;
		}
		if (numberOfKnots == 100) 
		{
			Melder_warning1 (L"drawSplines: too many knots (101)");
			return;
		}
	    knot[++numberOfKnots] = value;
	}

	numberOfInteriorKnots = numberOfKnots - k;
	for (i = 1; i <= k; i++)
	{
		knot[++numberOfKnots] = high;
	}
	
	nSplines = order + numberOfInteriorKnots;
	
	if (nSplines == 0) return;
	
	Graphics_setWindow (g, low, high, ymin, ymax);
	Graphics_setInner (g);
	for (i = 1; i <= nSplines; i++)
	{
		double x, yx, dx = (high-low) / (n-1);
		for (j = 1; j <= n; j++) 
		{
			x = low + dx * (j - 1);
			if (type == MDS_MSPLINE) 
			{
				(void) NUMmspline (knot, numberOfKnots, order, i, x, &yx);
			}
			else
			{
				(void) NUMispline (knot, numberOfKnots, order, i, x, &yx);
			}
			y[j] = yx < ymin ? ymin : yx > ymax ? ymax : yx;
		}
		Graphics_function (g, y, 1, n, low, high);
	} 
	Graphics_unsetInner (g);
	if (garnish)
	{
		static MelderString ts = { 0 };
		long lastKnot = type == MDS_ISPLINE ? numberOfKnots - 2 : numberOfKnots;
		MelderString_empty (&ts);
		Graphics_drawInnerBox (g);
	   	Graphics_textLeft (g, 0, type == MDS_MSPLINE ? L"\\s{M}\\--spline" : 
			L"\\s{I}\\--spline");
		Graphics_marksTop (g, 2, 1, 1, 0);
    	Graphics_marksLeft (g, 2, 1, 1, 0);
    	if (low <= knot[order])
    	{
    		if (order == 1) MelderString_append (&ts, L"t__1_");
			else if (order == 2) MelderString_append (&ts,  L"{t__1_, t__2_}");
			else MelderString_append3 (&ts, L"{t__1_..t__", Melder_integer (order), L"_}"); 
			Graphics_markBottom (g, low, 0, 0, 0, ts.string);
		}
		for (i = 1; i <= numberOfInteriorKnots; i++)
		{
			if (low <= knot[k+i] && knot[k+i] < high)
			{
				MelderString_empty (&ts);
				MelderString_append3 (&ts, L"t__", Melder_integer (order + i), L"_");
				Graphics_markBottom (g, knot[k+i], 0, 1, 1, ts.string); 
				Graphics_markTop (g, knot[k+i], 1, 0, 0, 0);
			}
		}
		if (knot[lastKnot-order+1] <= high)
		{
			MelderString_empty (&ts);
			if (order == 1)
			{
				MelderString_append3 (&ts, L"t__", Melder_integer (lastKnot), L"_");
			}
			else 
			{
				MelderString_append5 (&ts, L"{t__", Melder_integer (order == 2 ? lastKnot-1 : lastKnot-order+1), L"_, t__", Melder_integer (lastKnot), L"_}");
			}
			Graphics_markBottom (g, high, 0, 0, 0, ts.string);
		}
	}
}

void drawMDSClassRelations (Graphics g)
{
	long i, nBoxes = 6;
	double boxWidth = 0.3, boxWidth2 = boxWidth / 2, boxWidth3 = boxWidth / 3;
	double boxHeight = 0.1, boxHeight2 = boxHeight / 2;
	double boxHeight3 = boxHeight / 3;
	double r_mm = 3, dxt = 0.025, dyt = 0.03;
	double dboxx = 1 - 0.2 - 2 * boxWidth, dboxy = (1 - 4 * boxHeight) / 3;
	double x1, x2, xm, x23, x13, y1, y2, ym, y23, y13;
	double x[7] = {0.0, 0.2, 0.2, 0.7, 0.2, 0.7, 0.2}; /* left */
	double y[7] = {0.0, 0.9, 0.6, 0.6, 0.3, 0.3, 0.0}; /* bottom */
	wchar_t *text[7] = {L"", L"Confusion", L"Dissimilarity  %\\de__%%ij%_",
		L"Similarity", L"Distance  %d__%%ij%_, %d\\'p__%%ij%_", 
		L"ScalarProduct", L"Configuration"};

	Graphics_setWindow (g, -0.05, 1.05, -0.05, 1.05);	
	Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_HALF);	
	for (i=1; i <= nBoxes; i++)
	{
		x2 = x[i] + boxWidth; y2 = y[i] + boxHeight;
		xm = x[i] + boxWidth2; ym = y[i] + boxHeight2;
		Graphics_roundedRectangle (g, x[i], x2, y[i], y2, r_mm);
		Graphics_text (g, xm, ym, text[i]);
	}
	
	Graphics_setLineWidth (g, 2);
	Graphics_setTextAlignment (g, Graphics_LEFT, Graphics_BOTTOM);	

	/*
		Confusion to Dissimilarity
	*/

	xm = x[1] + boxWidth2;
	y2 = y[2] + boxHeight;
	Graphics_arrow (g, xm, y[1], xm, y2);
	Graphics_text (g, xm + dxt, y2 + dboxy / 2, L"pdf"); 

	/*
		Confusion to Similarity
	*/
	
	x1 = x[1] + boxWidth;
	xm = x[3] + boxWidth2;
	ym = y[1] + boxHeight2;
	y2 = y[3] + boxHeight;
	Graphics_line (g, x1, ym, xm, ym);
	Graphics_arrow (g, xm, ym, xm, y2);
	y2 += + dboxy / 2 + dyt / 2;
	Graphics_text (g, xm + dxt, y2, L"average"); 
	y2 -= dyt;
	Graphics_text (g, xm + dxt, y2, L"houtgast"); 

	/*
		Dissimilarity to Similarity
	*/
	
	x1 = x[2] + boxWidth;
	y23 = y[2] + 2 * boxHeight3;
	Graphics_arrow (g, x1, y23, x[3], y23);
	y13 = y[2] + boxHeight3;
	Graphics_arrow (g, x[3], y13, x1, y13);

	/*
		Dissimilarity to Distance
	*/
	
	x13 = x[4] + boxWidth3;
	y1 = y[4] + boxHeight;
	Graphics_arrow (g, x13, y1, x13, y[2]);
	x23 = x[4] + 2 * boxWidth3;
	Graphics_arrow (g, x23, y[2], x23, y1);
	
	x1 = x23 + dxt;
	y1 = y[2] - dyt;
	x2 = 0 + dxt;
	y1 -= dyt;
	Graphics_text (g, x1, y1, L"%d\\'p__%%ij%_ = %\\de__%%ij%_");
	Graphics_text (g, x2, y1, L"absolute");
	y1 -= dyt;
	Graphics_text (g, x1, y1, L"%d\\'p__%%ij%_ = %b\\.c%\\de__%%ij%_");
	Graphics_text (g, x2, y1, L"ratio");
	y1 -= dyt;
	Graphics_text (g, x1, y1, L"%d\\'p__%%ij%_ = %b\\.c%\\de__%%ij%_+%a");
	Graphics_text (g, x2, y1, L"interval");
	y1 -= dyt;
	Graphics_text (g, x1, y1, L"%d\\'p__%%ij%_ = \\s{I}-spline (%\\de__%%ij%_)");
	Graphics_text (g, x2, y1, L"\\s{I}\\--spline");
	y1 -= dyt;
	Graphics_text (g, x1, y1, L"%d\\'p__%%ij%_ = monotone (%\\de__%%ij%_)");
	Graphics_text (g, x2, y1, L"monotone");
	
	/*
		Distance to ScalarProduct
	*/
	
	x1 = x[4] + boxWidth;
	ym = y[4] + boxHeight2;		
	Graphics_arrow (g, x1, ym, x[5], ym);
	
	/*
		Distance to Configuration
	*/
	
	y1 = y[6] + boxHeight;
	Graphics_arrow (g, x13, y1, x13, y[4]);
		
	/*
		ScalarProduct to Configuration
	*/
	
	x13 = x[5] + boxWidth3;
	x23 = x[6] + 2 * boxWidth3;
	y1 = y[5] - dboxy / 2;
	Graphics_line (g, x13, y[5], x13, y1);
	Graphics_line (g, x13, y1, x23, y1);
	Graphics_arrow (g, x23, y1, x23, y[6] + boxHeight);
	x1 = x[6] + boxWidth + dboxx / 2;
	Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_BOTTOM);
	Graphics_text (g, x1, y1, L"\\s{TORSCA}");
	Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_TOP);
	Graphics_text (g, x1, y1, L"\\s{YTL}");
	
	Graphics_setLineType (g, Graphics_DOTTED);
	
	x23 = x[5] + 2 * boxWidth3;
	ym = y[6] + boxHeight2;	
	Graphics_line (g, x23, y[5], x23, ym);
	Graphics_arrow (g, x23, ym, x[6] + boxWidth, ym);
	x1 = x[6] + boxWidth + dboxx / 2 + boxWidth3;
	Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_BOTTOM);
	Graphics_text (g, x1, ym, L"\\s{INDSCAL}");
	
	/*
		Dissimilarity to Configuration
	*/

	ym = y[2] + boxHeight2;
	y2 = y[6] + boxHeight2;	
	Graphics_line (g, x[2], ym, 0, ym);
	Graphics_line (g, 0, ym, 0, y2);
	Graphics_arrow (g, 0, y2, x[6], y2);
	
	/*
		Restore settings
	*/
	
	Graphics_setLineType (g, Graphics_DRAWN);
	Graphics_setLineWidth (g, 1);
	Graphics_setTextAlignment (g, Graphics_LEFT, Graphics_BOTTOM);
	
}

FORM (drawSplines, L"Draw splines", L"spline")
	REAL (L"left Horizontal range", L"0.0")
	REAL (L"right Horizontal range", L"1.0")
	REAL (L"left Vertical range", L"0.0")
	REAL (L"right Vertical range", L"20.0")
	RADIO (L"Spline type", 1)
	RADIOBUTTON (L"M-spline")
	RADIOBUTTON (L"I-spline")
	INTEGER (L"Order", L"3")
	SENTENCE (L"Interior knots", L"0.3 0.5 0.6")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	double xmin = GET_REAL (L"left Horizontal range"), xmax = GET_REAL (L"right Horizontal range");
	double ymin = GET_REAL (L"left Vertical range"), ymax = GET_REAL (L"right Vertical range");
	REQUIRE (xmin < xmax && ymin < ymax, L"Required: xmin < xmax and ymin < ymax.")
	praat_picture_open ();
	drawSplines (GRAPHICS, xmin, xmax, ymin, ymax, GET_INTEGER (L"Spline type"),
		GET_INTEGER (L"Order"), GET_STRING (L"Interior knots"),
		GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

DIRECT (drawMDSClassRelations)
	praat_picture_open ();
	drawMDSClassRelations (GRAPHICS);
	praat_picture_close ();
END


/***************** AffineTransform ***************************************/


DIRECT (AffineTransform_help)
	Melder_help (L"AffineTransform");
END

DIRECT (AffineTransform_invert)
	EVERY_CHECK (praat_new2 (AffineTransform_invert (OBJECT), NAME, L"_inverse"))
END

FORM (AffineTransform_getTransformationElement, L"AffineTransform: Get transformation element", L"Procrustes")
	NATURAL (L"Row number", L"1")
	NATURAL (L"Column number", L"1")
	OK
DO
	AffineTransform me = (structAffineTransform *)ONLY_OBJECT;
	long row = GET_INTEGER (L"Row number");
	long column = GET_INTEGER (L"Column number");
	REQUIRE (row <= my n, L"Row number must not exceed number of rows.")
	REQUIRE (column <= my n, L"Column number must not exceed number of columns.")
	Melder_information1 (Melder_double (my r [row] [column]));
END

FORM (AffineTransform_getTranslationElement, L"AffineTransform: Get translation element", L"Procrustes")
	NATURAL (L"Index", L"1")
	OK
DO
	AffineTransform me = (structAffineTransform *)ONLY_OBJECT;
	long number = GET_INTEGER (L"Index");
	REQUIRE (number <= my n, L"Index must not exceed number of elements.")
	Melder_information1 (Melder_double (my t [number]));
END

DIRECT (AffineTransform_extractMatrix)
	EVERY_TO (AffineTransform_extractMatrix (OBJECT))
END

DIRECT (AffineTransform_extractTranslationVector)
	EVERY_TO (AffineTransform_extractTranslationVector (OBJECT))
END

/***************** Configuration ***************************************/

DIRECT (Configuration_help)
	Melder_help (L"Configuration");
END

static void Configuration_draw_addCommonFields (UiForm *dia)
{
	NATURAL (L"Horizontal dimension", L"1")
	NATURAL (L"Vertical dimension", L"2")
	REAL (L"left Horizontal range", L"0.0")
	REAL (L"right Horizontal range", L"0.0")
	REAL (L"left Vertical range", L"0.0")
	REAL (L"right Vertical range", L"0.0")
}

void Configuration_draw (Configuration me, Graphics g, int xCoordinate,
	int yCoordinate, double xmin, double xmax, double ymin, double ymax,
	int labelSize, int useRowLabels, wchar_t *label, int garnish)
{
	long i, nPoints = my numberOfRows, numberOfDimensions = my numberOfColumns;
	double *x = NULL, *y = NULL;
	int fontSize = Graphics_inqFontSize (g), noLabel = 0;

	if (numberOfDimensions > 1 && (xCoordinate > numberOfDimensions ||
		yCoordinate > numberOfDimensions)) return;
	if (numberOfDimensions == 1) xCoordinate = 1;
	if (labelSize == 0) labelSize = fontSize;
	if (! (x = NUMdvector (1, nPoints)) ||
		! (y = NUMdvector (1, nPoints))) goto end;

	for (i = 1; i <= nPoints; i++)
	{
		x[i] = my data[i][xCoordinate] * my w[xCoordinate];
		y[i] = numberOfDimensions > 1 ?
			my data[i][yCoordinate] * my w[yCoordinate] : 0;
	}
	if (xmax <= xmin) NUMdvector_extrema (x, 1, nPoints, &xmin, &xmax);
	if (xmax <= xmin) { xmax += 1; xmin -= 1; }
	if (ymax <= ymin) NUMdvector_extrema (y, 1, nPoints, &ymin, &ymax);
	if (ymax <= ymin) { ymax += 1; ymin -= 1; }
    Graphics_setWindow (g, xmin, xmax, ymin, ymax);
    Graphics_setInner (g);
    Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_HALF);
	Graphics_setFontSize (g, labelSize);
	for (i = 1; i <= my numberOfRows; i++)
	{
		if (x[i] >= xmin && x[i] <= xmax && y[i] >= ymin && y[i] <= ymax)
		{
			wchar_t *plotLabel = useRowLabels ? my rowLabels[i] : label;
			if (NUMstring_containsPrintableCharacter (plotLabel))
			{
				Graphics_text (g, x[i], y[i], plotLabel);
			}
			else noLabel++;
		}
	}
	Graphics_setFontSize (g, fontSize);
	Graphics_setTextAlignment (g, Graphics_LEFT, Graphics_BOTTOM);
	Graphics_unsetInner (g);
	if (garnish)
	{
		Graphics_drawInnerBox (g);
		Graphics_marksBottom (g, 2, 1, 1, 0);
    	if (numberOfDimensions > 1)
    	{
    		Graphics_marksLeft (g, 2, 1, 1, 0);
    		if (my columnLabels[xCoordinate])
    			Graphics_textBottom (g, 1, my columnLabels[xCoordinate]);
			if (my columnLabels[yCoordinate])
				Graphics_textLeft (g, 1, my columnLabels[yCoordinate]);
    	}
	}

	if (noLabel > 0) Melder_warning5 (L"Configuration_draw: ", Melder_integer (noLabel), L" from ", Melder_integer (my numberOfRows),
		L" labels are not visible because they are empty or they contain only spaces or they contain only non-printable characters");

end:

	NUMdvector_free (x, 1);
	NUMdvector_free (y, 1);
}

void Configuration_drawConcentrationEllipses (Configuration me, Graphics g,
	double scale, int confidence, wchar_t *label, long d1, long d2, double xmin, double xmax,
	double ymin, double ymax, int fontSize, int garnish)
{
	SSCPs sscps = TableOfReal_to_SSCPs_byLabel (me);

	if (sscps == NULL) return;

	SSCPs_drawConcentrationEllipses (sscps, g, scale, confidence, label,
			d1, d2, xmin, xmax, ymin, ymax, fontSize, garnish);

	forget (sscps);
}

FORM (Configuration_draw, L"Configuration: Draw", L"Configuration: Draw...")
	Configuration_draw_addCommonFields (dia);
	NATURAL (L"Label size", L"12")
	BOOLEAN (L"Use row labels", 0)
	WORD (L"Label", L"+")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Configuration_draw ((structConfiguration *)OBJECT, GRAPHICS,
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"), GET_REAL (L"left Vertical range"),
		GET_REAL (L"right Vertical range"), GET_INTEGER (L"Label size"),
		GET_INTEGER (L"Use row labels"), GET_STRING (L"Label"),
		GET_INTEGER (L"Garnish")))
END

FORM (Configuration_drawSigmaEllipses, L"Configuration: Draw sigma ellipses", L"Configuration: Draw sigma ellipses...")
	POSITIVE (L"Number of sigmas", L"1.0")
	Configuration_draw_addCommonFields (dia);
	INTEGER (L"Label size", L"12")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Configuration_drawConcentrationEllipses ((structConfiguration *)OBJECT, GRAPHICS,
		GET_REAL (L"Number of sigmas"), 0, NULL,
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"),
		GET_REAL (L"left Vertical range"), GET_REAL (L"right Vertical range"),
		GET_INTEGER (L"Label size"), GET_INTEGER (L"Garnish")))
END

FORM (Configuration_drawOneSigmaEllipse, L"Configuration: Draw one sigma ellipse", L"Configuration: Draw sigma ellipses...")
	SENTENCE (L"Label", L"ui/editors/AmplitudeTierEditor.h")
	POSITIVE (L"Number of sigmas", L"1.0")
	Configuration_draw_addCommonFields (dia);
	INTEGER (L"Label size", L"12")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Configuration_drawConcentrationEllipses ((structConfiguration *)OBJECT, GRAPHICS,
		GET_REAL (L"Number of sigmas"), 0, GET_STRING (L"Label"),
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"),
		GET_REAL (L"left Vertical range"), GET_REAL (L"right Vertical range"),
		GET_INTEGER (L"Label size"), GET_INTEGER (L"Garnish")))
END


FORM (Configuration_drawConfidenceEllipses, L"Configuration: Draw confidence ellipses", 0)
	POSITIVE (L"Confidence level (0-1)", L"0.95")
	Configuration_draw_addCommonFields (dia);
	INTEGER (L"Label size", L"12")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Configuration_drawConcentrationEllipses ((structConfiguration *)OBJECT, GRAPHICS,
		GET_REAL (L"Confidence level"), 1, NULL,
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"),
		GET_REAL (L"left Vertical range"), GET_REAL (L"right Vertical range"),
		GET_INTEGER (L"Label size"), GET_INTEGER (L"Garnish")))
END

FORM (Configuration_drawOneConfidenceEllipse, L"Configuration: Draw one confidence ellipse", 0)
	SENTENCE (L"Label", L"ui/editors/AmplitudeTierEditor.h")
	POSITIVE (L"Confidence level (0-1)", L"0.95")
	Configuration_draw_addCommonFields (dia);
	INTEGER (L"Label size", L"12")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Configuration_drawConcentrationEllipses ((structConfiguration *)OBJECT, GRAPHICS,
		GET_REAL (L"Confidence level"), 1, GET_STRING (L"Label"),
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_REAL (L"left Horizontal range"), GET_REAL (L"right Horizontal range"),
		GET_REAL (L"left Vertical range"), GET_REAL (L"right Vertical range"),
		GET_INTEGER (L"Label size"), GET_INTEGER (L"Garnish")))
END

DIRECT (Configuration_randomize)
	EVERY (Configuration_randomize ((structConfiguration *)OBJECT))
END

FORM (Configuration_normalize, L"Configuration: Normalize", L"Configuration: Normalize...")
	REAL (L"Sum of squares", L"0.0")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"On (INDSCAL), Off (Kruskal)")
	BOOLEAN (L"Each dimension separately", 1)
	OK
DO
	EVERY (Configuration_normalize ((structConfiguration *)OBJECT, GET_REAL (L"Sum of squares"),
		GET_INTEGER (L"Each dimension separately")))
END

DIRECT (Configuration_centralize)
	EVERY (TableOfReal_centreColumns (OBJECT))
END

FORM (Configuration_rotate, L"Configuration: Rotate", L"Configuration: Rotate...")
	NATURAL (L"Dimension 1", L"1")
	NATURAL (L"Dimension 2", L"2")
	REAL (L"Angle (degrees)", L"60.0")
	OK
DO
	EVERY (Configuration_rotate ((structConfiguration *)OBJECT, GET_INTEGER (L"Dimension 1"),
		GET_INTEGER (L"Dimension 2"), GET_REAL (L"Angle")))
END

DIRECT (Configuration_rotateToPrincipalDirections)
	EVERY (Configuration_rotateToPrincipalDirections ((structConfiguration *)OBJECT))
END

FORM (Configuration_invertDimension, L"Configuration: Invert dimension", L"Configuration: Invert dimension...")
	NATURAL (L"Dimension", L"1")
	OK
DO
	EVERY (Configuration_invertDimension ((structConfiguration *)OBJECT, GET_INTEGER (L"Dimension")))
END

DIRECT (Configuration_to_Distance)
	EVERY_TO (Configuration_to_Distance ((structConfiguration *)OBJECT))
END

FORM (Configuration_varimax, L"Configuration: To Configuration (varimax)", L"Configuration: To Configuration (varimax)...")
	BOOLEAN (L"Normalize rows", 1)
	BOOLEAN (L"Quartimax", 0)
	NATURAL (L"Maximum number of iterations", L"50")
	POSITIVE (L"Tolerance", L"1e-6")
	OK
DO
	EVERY_TO (Configuration_varimax ((structConfiguration *)OBJECT, GET_INTEGER (L"Normalize rows"),
		GET_INTEGER (L"Quartimax"),
		GET_INTEGER (L"Maximum number of iterations"), GET_REAL (L"Tolerance")))
END

DIRECT (Configuration_to_Similarity_cc)
	Configurations cs = Configurations_create();
	Similarity s = NULL;
	WHERE (SELECTED) (void) Collection_addItem (cs, OBJECT);
	s = Configurations_to_Similarity_cc (cs, NULL);
	cs -> size = 0; forget (cs);
	if (! praat_new1 (s, L"congruence")) return 0;
END

FORM (Configurations_to_Procrustes, L"Configuration & Configuration: To Procrustes", L"Configuration & Configuration: To Procrustes...")
	BOOLEAN (L"Orthogonal transform", 0)
	OK
DO
	Configuration c1 = NULL, c2 = NULL;
	WHERE (SELECTED) { if (c1) c2 = (structConfiguration *)OBJECT; else c1 = (structConfiguration *)OBJECT; }
	if (! praat_new3 (Configurations_to_Procrustes (c1, c2, GET_INTEGER (L"Orthogonal transform")),
		Thing_getName (c2), L"_to_", Thing_getName (c1))) return 0;
END

FORM (Configurations_to_AffineTransform_congruence, L"Configurations: To AffineTransform (congruence)", L"Configurations: To AffineTransform (congruence)...")
	NATURAL (L"Maximum number of iterations", L"50")
	POSITIVE (L"Tolerance", L"1e-6")
	OK
DO
	Configuration c1 = NULL, c2 = NULL;
	WHERE (SELECTED && CLASS == classConfiguration)
	{
		if (c1) c2 = (structConfiguration *)OBJECT;
		else c1 = (structConfiguration *)OBJECT;
	}
	NEW (Configurations_to_AffineTransform_congruence (c1, c2,
		GET_INTEGER (L"Maximum number of iterations"),
		GET_REAL (L"Tolerance")))
END

DIRECT (Configuration_Weight_to_Similarity_cc)
	Configurations cs = Configurations_create();
	Similarity s = NULL; Weight w = (structWeight *)ONLY (classWeight);
	WHERE (SELECTED && CLASS == classConfiguration)
	{
		(void) Collection_addItem (cs, OBJECT);
	}
	s = Configurations_to_Similarity_cc (cs, w);
	cs -> size = 0; forget (cs);
	if (! praat_new1 (s, L"congruence")) return 0;
END

DIRECT (Configuration_and_AffineTransform_to_Configuration)
	NEW (Configuration_and_AffineTransform_to_Configuration
		((structConfiguration *)ONLY (classConfiguration), ONLY_GENERIC (classAffineTransform)))
END

/*************** Confusion *********************************/

FORM (Confusion_to_Dissimilarity_pdf, L"Confusion: To Dissimilarity (pdf)", L"Confusion: To Dissimilarity (pdf)...")
	POSITIVE (L"Minimum confusion level", L"0.5")
	OK
DO
	WHERE (SELECTED)
	{
		Confusion c = (structConfusion *)OBJECT;
		if (! praat_new2 (Confusion_to_Dissimilarity_pdf ((structConfusion *)OBJECT,
			GET_REAL (L"Minimum confusion level")), c -> name, L"_pdf"))
				return 0;
	}
END

FORM (Confusion_to_Similarity, L"Confusion: To Similarity", L"Confusion: To Similarity...")
	BOOLEAN (L"Normalize", 1)
	RADIO (L"Symmetrization", 1)
	RADIOBUTTON (L"No symmetrization")
	RADIOBUTTON (L"Average (s[i][j] = (c[i][j]+c[j][i])/2)")
	RADIOBUTTON (L"Houtgast (s[i][j]= sum (min(c[i][k],c[j][k])))")
	OK
DO
	EVERY_TO (Confusion_to_Similarity ((structConfusion *)OBJECT, GET_INTEGER (L"Normalize"),
		GET_INTEGER (L"Symmetrization")))
END

DIRECT (Confusions_sum)
	Confusions me = Confusions_create(); Confusion conf = NULL;
	WHERE (SELECTED) (void) Collection_addItem (me, OBJECT);
	conf = Confusions_sum (me);
	my size = 0; forget (me);
	if (! praat_new1 (conf, L"untitled_sum")) return 0;
END

DIRECT (Confusion_to_ContingencyTable)
	EVERY_TO (Confusion_to_ContingencyTable ((structConfusion *)OBJECT))
END

/*************** ContingencyTable *********************************/


FORM (ContingencyTable_to_Configuration_ca, L"ContingencyTable: To Configuration (ca)", L"ContingencyTable: To Configuration (ca)...")
	NATURAL (L"Number of dimensions", L"2")
	RADIO (L"Scaling of final configuration", 3)
	RADIOBUTTON (L"Row points in centre of gravity of column points")
	RADIOBUTTON (L"Column points in centre of gravity of row points")
	RADIOBUTTON (L"Row points and column points symmetric")
	OK
DO
	EVERY_TO (ContingencyTable_to_Configuration_ca ((structContingencyTable *)OBJECT,
		GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Scaling of final configuration")))
END

DIRECT (ContingencyTable_chisqProbability)
	Melder_information1 (Melder_double (ContingencyTable_chisqProbability ((structContingencyTable *)ONLY_OBJECT)));
END

DIRECT (ContingencyTable_cramersStatistic)
	Melder_information1 (Melder_double (ContingencyTable_cramersStatistic ((structContingencyTable *)ONLY_OBJECT)));
END

DIRECT (ContingencyTable_contingencyCoefficient)
	Melder_information1 (Melder_double (ContingencyTable_contingencyCoefficient ((structContingencyTable *)ONLY_OBJECT)));
END

/************************* Correlation ***********************************/

FORM (Correlation_to_Configuration, L"Correlation: To Configuration", 0)
	NATURAL (L"Number of dimensions", L"2")
	OK
DO
	EVERY_TO (Correlation_to_Configuration ((structCorrelation *)OBJECT,
		GET_INTEGER (L"Number of dimensions")))
END


/************************* Similarity ***************************************/

DIRECT (Similarity_help)
	Melder_help (L"Similarity");
END

FORM (Similarity_to_Dissimilarity, L"Similarity: To Dissimilarity", L"Similarity: To Dissimilarity...")
	REAL (L"Maximum dissimilarity", L"0.0 (=from data)")
	OK
DO
	EVERY_TO (Similarity_to_Dissimilarity ((structSimilarity *)OBJECT, GET_REAL (L"Maximum dissimilarity")))
END

/**************** Dissimilarity ***************************************/

static void Dissimilarity_to_Configuration_addCommonFields (UiForm *dia)
{
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Minimization parameters")
	REAL (L"Tolerance", L"1e-5")
	NATURAL (L"Maximum number of iterations", L"50 (= each repetition)")
	NATURAL (L"Number of repetitions", L"1")
}

static void Dissimilarity_and_Configuration_getStress_addCommonFields (UiForm *dia, UiForm::UiField *radio)
{
	RADIO (L"Stress measure", 1)
	RADIOBUTTON (L"Normalized")
	RADIOBUTTON (L"Kruskal's stress-1")
	RADIOBUTTON (L"Kruskal's stress-2")
	RADIOBUTTON (L"Raw")
}

static void Dissimilarity_Configuration_drawDiagram_addCommonFields (UiForm *dia)
{
	REAL (L"left Proximity range", L"0.0")
	REAL (L"right Proximity range", L"0.0")
	REAL (L"left Distance range", L"0.0")
	REAL (L"right Distance range", L"0.0")
	POSITIVE (L"Mark size (mm)", L"1.0")
	SENTENCE (L"Mark string (+xo.)", L"+")
	BOOLEAN (L"Garnish", 1)
}

void Dissimilarity_Configuration_drawShepardDiagram (Dissimilarity me, 
	Configuration him, Graphics g, double xmin, double xmax, double ymin, 
	double ymax, double size_mm, const wchar_t *mark, int garnish)
{
	Distance dist = Configuration_to_Distance (him);
	 
	if (dist == NULL) return;
	Proximity_Distance_drawScatterDiagram (me, dist, g, xmin, xmax, 
		ymin, ymax, size_mm, mark, garnish);
	forget (dist);
}

void Dissimilarity_Configuration_drawMonotoneRegression 
	(Dissimilarity me, Configuration him, Graphics g, int tiesProcessing, 
	double xmin, double xmax, double ymin, double ymax, double size_mm, 
	const wchar_t *mark, int garnish)
{/* obsolete replace by transformator */
	Distance fit = Dissimilarity_Configuration_monotoneRegression 
		(me, him, tiesProcessing);
	if (! fit) return;
	Proximity_Distance_drawScatterDiagram (me, fit, g, xmin, xmax, ymin, ymax, 
		size_mm, mark, garnish);
	forget (fit);
}

void Dissimilarity_Configuration_Weight_drawAbsoluteRegression 
	(Dissimilarity d, Configuration c, Weight w, Graphics g, 
	double xmin, double xmax, double ymin, double ymax, 
	double size_mm, const wchar_t *mark, int garnish)
{
	Distance fit = NULL;
	Transformator t = Transformator_create (d->numberOfRows);
	
	if (t == NULL) return;
	
	fit = Dissimilarity_Configuration_Transformator_Weight_transform 
		(d, c, t, w);
	forget (t);
	
	if (fit)
	{
		Proximity_Distance_drawScatterDiagram 
			(d, fit, g, xmin, xmax, ymin, ymax, size_mm, mark, garnish);
		forget (fit);
	}
}

void Dissimilarity_Configuration_Weight_drawRatioRegression (Dissimilarity d,
	Configuration c, Weight w, Graphics g, 
	double xmin, double xmax, double ymin, double ymax,
	double size_mm, const wchar_t *mark, int garnish)
{
	Distance fit = NULL; 
	RatioTransformator t = RatioTransformator_create (d -> numberOfRows);
	
	if (t == NULL) return;
	
	fit = Dissimilarity_Configuration_Transformator_Weight_transform 
		(d, c, t, w);
	forget (t);
	
	if (fit)
	{
		Proximity_Distance_drawScatterDiagram 
			(d, fit, g, xmin, xmax, ymin, ymax, size_mm, mark, garnish);
		forget (fit);
	}	
}

void Dissimilarity_Configuration_Weight_drawIntervalRegression (Dissimilarity d,
	Configuration c, Weight w, Graphics g, 
	double xmin, double xmax, double ymin, double ymax,
	double size_mm, const wchar_t *mark, int garnish)
{
	Dissimilarity_Configuration_Weight_drawISplineRegression (d, c, w, g,
		0, 1, xmin, xmax, ymin, ymax, size_mm, mark, garnish);
}

void Dissimilarity_Configuration_Weight_drawMonotoneRegression (Dissimilarity d,
	Configuration c, Weight w, Graphics g, int tiesProcessing, 
	double xmin, double xmax, double ymin, double ymax,
	double size_mm, const wchar_t *mark, int garnish)
{
	Distance fit; 
	MonotoneTransformator t = MonotoneTransformator_create (d->numberOfRows);
	
	if (t == NULL) return;
	
	MonotoneTransformator_setTiesProcessing (t, tiesProcessing);
	
	fit = Dissimilarity_Configuration_Transformator_Weight_transform 
		(d, c, t, w);
	forget (t);
	
	if (fit)
	{
		Proximity_Distance_drawScatterDiagram 
			(d, fit, g, xmin, xmax, ymin, ymax, size_mm, mark, garnish);
		forget (fit);
	}
}

void Dissimilarity_Configuration_Weight_drawISplineRegression 
	(Dissimilarity d, Configuration c, Weight w, Graphics g,
	long numberOfInternalKnots, long order, double xmin, double xmax, 
	double ymin, double ymax, double size_mm, const wchar_t *mark, int garnish)
{
	Distance fit; 
	ISplineTransformator t = ISplineTransformator_create (d->numberOfRows, 
		numberOfInternalKnots, order);
		
	if (t == NULL) return;
	
	fit = Dissimilarity_Configuration_Transformator_Weight_transform 
		(d, c, t, w);
	forget (t);
	
	if (fit)
	{
		Proximity_Distance_drawScatterDiagram 
			(d, fit, g, xmin, xmax, ymin, ymax, size_mm, mark, garnish);
		forget (fit);
	}
}

DIRECT (Dissimilarity_help)
	Melder_help (L"Dissimilarity");
END

DIRECT (Dissimilarity_getAdditiveConstant)
	double c;
	Dissimilarity_getAdditiveConstant(ONLY_OBJECT, &c);
	Melder_information1 (Melder_double (c));
END

FORM (Dissimilarity_Configuration_kruskal, L"Dissimilarity & Configuration: To Configuration (kruskal)", L"Dissimilarity & Configuration: To Configuration (kruskal)...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	RADIO (L"Stress calculation", 1)
	RADIOBUTTON (L"Formula1 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k])^2))")
	RADIOBUTTON (L"Formula2 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k]-dbar)^2))")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Configuration conf = (structConfiguration *)ONLY (classConfiguration);
	if (! praat_new2 (Dissimilarity_Configuration_kruskal
		((structDissimilarity *)ONLY (classDissimilarity), conf,
		GET_INTEGER (L"Handling of ties"), GET_INTEGER (L"Stress calculation"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions")), conf -> name, L"_s_kruskal")) return 0;
END

FORM (Dissimilarity_Configuration_absolute_mds, L"Dissimilarity & Configuration: To Configuration (absolute mds)", L"Dissimilarity & Configuration: To Configuration (absolute mds)...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_absolute_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_s_absolute")) return 0;
END

FORM (Dissimilarity_Configuration_ratio_mds, L"Dissimilarity & Configuration: To Configuration (ratio mds)", L"Dissimilarity & Configuration: To Configuration (ratio mds)...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_ratio_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_s_ratio")) return 0;
END

FORM (Dissimilarity_Configuration_interval_mds, L"Dissimilarity & Configuration: To Configuration (interval mds)", L"Dissimilarity & Configuration: To Configuration (interval mds)...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_interval_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_s_interval")) return 0;
END

FORM (Dissimilarity_Configuration_monotone_mds, L"Dissimilarity & Configuration: To Configuration (monotone mds)", L"Dissimilarity & Configuration: To Configuration (monotone mds)...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_monotone_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Handling of ties"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_s_monotone")) return 0;
END

FORM (Dissimilarity_Configuration_ispline_mds, L"Dissimilarity & Configuration: To Configuration (i-spline mds)", L"Dissimilarity & Configuration: To Configuration (i-spline mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Spline smoothing")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"1")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_ispline_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Number of interior knots"),
		GET_INTEGER (L"Order of I-spline"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_s_ispline")) return 0;
END

FORM (Dissimilarity_Configuration_Weight_absolute_mds, L"Dissimilarity & Configuration & Weight: To Configuration (absolute mds)", L"Dissimilarity & Configuration & Weight: To Configuration...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_absolute_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), (structWeight *)ONLY (classWeight),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_sw_absolute")) return 0;
END

FORM (Dissimilarity_Configuration_Weight_ratio_mds, L"Dissimilarity & Configuration & Weight: To Configuration (ratio mds)", L"Dissimilarity & Configuration & Weight: To Configuration...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_ratio_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), (structWeight *)ONLY (classWeight),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_sw_ratio")) return 0;
END

FORM (Dissimilarity_Configuration_Weight_interval_mds, L"Dissimilarity & Configuration & Weight: To Configuration (interval mds)", L"Dissimilarity & Configuration & Weight: To Configuration...")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_interval_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), (structWeight *)ONLY (classWeight),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_sw_interval")) return 0;
END

FORM (Dissimilarity_Configuration_Weight_monotone_mds,
	L"Dissimilarity & Configuration & Weight: To Configuration (monotone mds)",
	L"Dissimilarity & Configuration & Weight: To Configuration...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_monotone_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), (structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Handling of ties"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_sw_monotone")) return 0;
END

FORM (Dissimilarity_Configuration_Weight_ispline_mds,
	L"Dissimilarity & Configuration & Weight: To Configuration (i-spline mds)",
	L"Dissimilarity & Configuration & Weight: To Configuration...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Spline smoothing")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"1")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Configuration_Weight_ispline_mds
		(dissimilarity, (structConfiguration *)ONLY (classConfiguration), (structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of interior knots"),
		GET_INTEGER (L"Order of I-spline"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_sw_ispline")) return 0;
END

FORM (Dissimilarity_Configuration_getStress, L"Dissimilarity & Configuration: Get stress",
	L"Dissimilarity & Configuration: get stress")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	RADIO (L"Stress calculation", 1)
	RADIOBUTTON (L"Formula1 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k])^2))")
	RADIOBUTTON (L"Formula2 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k]-dbar)^2))")
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_getStress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration),
		GET_INTEGER (L"Handling of ties"), GET_INTEGER (L"Stress calculation"))));
END

FORM (Dissimilarity_Configuration_absolute_stress,
	L"Dissimilarity & Configuration: Get stress (absolute mds)",
	L"Dissimilarity & Configuration: Get stress (absolute mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_absolute_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_ratio_stress, L"Dissimilarity & Configuration: Get stress (ratio mds)",
	L"Dissimilarity & Configuration: Get stress (ratio mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_ratio_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_interval_stress,
	L"Dissimilarity & Configuration: Get stress (interval mds)",
	L"Dissimilarity & Configuration: Get stress (interval mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_interval_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_monotone_stress,
	L"Dissimilarity & Configuration: Get stress (monotone mds)",
	L"Dissimilarity & Configuration: Get stress (monotone mds)...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_monotone_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Handling of ties"), GET_INTEGER (L"Stress measure"))));
END


FORM (Dissimilarity_Configuration_ispline_stress,
	L"Dissimilarity & Configuration: Get stress (i-spline mds)",
	L"Dissimilarity & Configuration: Get stress (i-spline mds)...")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"3")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_ispline_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Number of interior knots"),
		GET_INTEGER (L"Order of I-spline"),
		GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_Weight_absolute_stress,
	L"Dissimilarity & Configuration & Weight: Get stress (absolute mds)",
	L"Dissimilarity & Configuration & Weight: Get stress (absolute mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_absolute_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration),
		(structWeight *)ONLY (classWeight), GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_Weight_ratio_stress,
	L"Dissimilarity & Configuration & Weight: Get stress (ratio mds)",
	L"Dissimilarity & Configuration & Weight: Get stress (ratio mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_ratio_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration),
		(structWeight *)ONLY (classWeight), GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_Weight_interval_stress,
	L"Dissimilarity & Configuration & Weight: Get stress (interval mds)",
	L"Dissimilarity & Configuration & Weight: Get stress (interval mds)...")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_interval_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration),
		(structWeight *)ONLY (classWeight), GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_Weight_monotone_stress,
	L"Dissimilarity & Configuration & Weight: Get stress (monotone mds)",
	L"Dissimilarity & Configuration & Weight: Get stress (monotone mds)...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach)")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_monotone_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration),
		(structWeight *)ONLY (classWeight), GET_INTEGER (L"Handling of ties"),
		GET_INTEGER (L"Stress measure"))));
END


FORM (Dissimilarity_Configuration_Weight_ispline_stress,
	L"Dissimilarity & Configuration & Weight: Get stress (i-spline mds)",
	L"Dissimilarity & Configuration & Weight: Get stress (i-spline mds)...")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"3")
	Dissimilarity_and_Configuration_getStress_addCommonFields (dia, radio);
	OK
DO
	Melder_information1 (Melder_double (Dissimilarity_Configuration_Weight_ispline_stress
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GET_INTEGER (L"Number of interior knots"),
		GET_INTEGER (L"Order of I-spline"),
		GET_INTEGER (L"Stress measure"))));
END

FORM (Dissimilarity_Configuration_drawShepardDiagram, L"Dissimilarity & Configuration: Draw Shepard diagram",
	L"Dissimilarity & Configuration: Draw Shepard diagram...")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_drawShepardDiagram ((structDissimilarity *)ONLY (classDissimilarity),
		(structConfiguration *)ONLY (classConfiguration), GRAPHICS,
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"),
		GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_Configuration_drawAbsoluteRegression,
	L"Dissimilarity & Configuration: Draw regression (absolute mds)",
	L"Dissimilarity & Configuration: Draw regression (absolute mds)...")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_Weight_drawAbsoluteRegression
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL, GRAPHICS,
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"),
		GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_Configuration_drawRatioRegression,
	L"Dissimilarity & Configuration: Draw regression (ratio mds)",
	L"Dissimilarity & Configuration: Draw regression (ratio mds)...")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_Weight_drawRatioRegression
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL, GRAPHICS,
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"), GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_Configuration_drawIntervalRegression,
	L"Dissimilarity & Configuration: Draw regression (interval mds)",
	L"Dissimilarity & Configuration: Draw regression (interval mds)...")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_Weight_drawIntervalRegression
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL, GRAPHICS,
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"), GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_Configuration_drawMonotoneRegression,
	L"Dissimilarity & Configuration: Draw regression (monotone mds)",
	L"Dissimilarity & Configuration: Draw regression (monotone mds)...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach)")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_Weight_drawMonotoneRegression
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL,
		GRAPHICS, GET_INTEGER (L"Handling of ties"),
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"), GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_Configuration_drawISplineRegression,
	L"Dissimilarity & Configuration: Draw regression (i-spline mds)",
	L"Dissimilarity & Configuration: Draw regression (i-spline mds)...")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"3")
	Dissimilarity_Configuration_drawDiagram_addCommonFields (dia);
	OK
DO
	praat_picture_open ();
	Dissimilarity_Configuration_Weight_drawISplineRegression
		((structDissimilarity *)ONLY (classDissimilarity), (structConfiguration *)ONLY (classConfiguration), NULL, GRAPHICS,
		GET_INTEGER (L"Number of interior knots"),
		GET_INTEGER (L"Order of I-spline"),
		GET_REAL (L"left Proximity range"), GET_REAL (L"right Proximity range"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"),
		GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Dissimilarity_kruskal, L"Dissimilarity: To Configuration (kruskal)",
	L"Dissimilarity: To Configuration (kruskal)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	NATURAL (L"Distance metric", L"2 (=Euclidean)")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	RADIO (L"Stress calculation", 1)
	RADIOBUTTON (L"Formula1 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k])^2))")
	RADIOBUTTON (L"Formula2 (sqrt (SUM((dist[k]-fit[k])^2) / SUM((dist[k]-dbar)^2))")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	if (! praat_new1 (Dissimilarity_kruskal (dissimilarity,
		 GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Distance metric"), GET_INTEGER (L"Handling of ties"),
		GET_INTEGER (L"Stress calculation"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions")),
		dissimilarity -> name)) return 0;
END

FORM (Dissimilarity_absolute_mds, L"Dissimilarity: To Configuration (absolute mds)",
	L"Dissimilarity: To Configuration (absolute mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_absolute_mds (dissimilarity, NULL,
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_absolute")) return 0;
END

FORM (Dissimilarity_ratio_mds, L"Dissimilarity: To Configuration (ratio mds)",
	L"Dissimilarity: To Configuration (ratio mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_ratio_mds (dissimilarity, NULL,
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_ratio")) return 0;
END

FORM (Dissimilarity_interval_mds, L"Dissimilarity: To Configuration (interval mds)",
	L"Dissimilarity: To Configuration (interval mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_interval_mds (dissimilarity, NULL,
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_interval")) return 0;
END

FORM (Dissimilarity_monotone_mds, L"Dissimilarity: To Configuration (monotone mds)",
	L"Dissimilarity: To Configuration (monotone mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_monotone_mds (dissimilarity, NULL,
		GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Handling of ties"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_monotone")) return 0;
END

FORM (Dissimilarity_ispline_mds, L"Dissimilarity: To Configuration (i-spline mds)",
	L"Dissimilarity: To Configuration (i-spline mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Spline smoothing")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"1")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	long niknots = GET_INTEGER (L"Number of interior knots");
	long order = GET_INTEGER (L"Order of I-spline");
	REQUIRE (order > 0 || niknots > 0,
		L"Order-zero spline must at least have 1 interior knot.")
	if (! praat_new2 (Dissimilarity_Weight_ispline_mds (dissimilarity, NULL,
		GET_INTEGER (L"Number of dimensions"),
		niknots, order,
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_ispline")) return 0;
END

FORM (Dissimilarity_Weight_ispline_mds, L"Dissimilarity & Weight: To Configuration (i-spline mds)",
	L"Dissimilarity & Weight: To Configuration (i-spline mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Spline smoothing")
	INTEGER (L"Number of interior knots", L"1")
	INTEGER (L"Order of I-spline", L"1")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	long niknots = GET_INTEGER (L"Number of interior knots");
	long order = GET_INTEGER (L"Order of I-spline");
	REQUIRE (order > 0 || niknots > 0,
		L"Order-zero spline must at least have 1 interior knot.")
	if (! praat_new2 (Dissimilarity_Weight_ispline_mds (dissimilarity,
		(structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of dimensions"), niknots, order,
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_ispline")) return 0;
END

FORM (Dissimilarity_Weight_absolute_mds, L"Dissimilarity & Weight: To Configuration (absolute mds)",
	L"Dissimilarity & Weight: To Configuration (absolute mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_absolute_mds (dissimilarity,
		(structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_absolute")) return 0;
END

FORM (Dissimilarity_Weight_ratio_mds, L"Dissimilarity & Weight: To Configuration (ratio mds)",
	L"Dissimilarity & Weight: To Configuration (ratio mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_ratio_mds (dissimilarity,
		(structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_absolute")) return 0;
END

FORM (Dissimilarity_Weight_interval_mds, L"Dissimilarity & Weight: To Configuration (interval mds)",
	L"Dissimilarity & Weight: To Configuration (interval mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_interval_mds (dissimilarity,
		(structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of dimensions"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_absolute")) return 0;
END

FORM (Dissimilarity_Weight_monotone_mds, L"Dissimilarity & Weight: To Configuration (monotone mds)",
	L"Dissimilarity & Weight: To Configuration (monotone mds)...")
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Configuration")
	NATURAL (L"Number of dimensions", L"2")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	Dissimilarity_to_Configuration_addCommonFields (dia);
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	int showProgress = 1;
	if (! praat_new2 (Dissimilarity_Weight_monotone_mds (dissimilarity,
		(structWeight *)ONLY (classWeight),
		GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Handling of ties"),
		GET_REAL (L"Tolerance"), GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"), showProgress),
		dissimilarity -> name, L"_monotone")) return 0;
END

FORM (Dissimilarity_to_Distance, L"Dissimilarity: To Distance", L"Dissimilarity: To Distance...")
	BOOLEAN (L"Scale (additive constant)", 1)
	OK
DO
	EVERY_TO (Dissimilarity_to_Distance ((structDissimilarity *)OBJECT, GET_INTEGER (L"Scale")))
END

DIRECT (Dissimilarity_to_Weight)
	EVERY_TO (Dissimilarity_to_Weight ((structDissimilarity *)OBJECT))
END

/************************* Distance(s) ***************************************/

void Proximity_Distance_drawScatterDiagram (I, Distance thee, Graphics g, 
	double xmin, double xmax, double ymin, double ymax, double size_mm, 
	const wchar_t *mark, int garnish)   
{
	iam (Proximity);
	long i, j, n = my numberOfRows * (my numberOfRows - 1) / 2;
	double **x = my data, **y = thy data;
	
	if (n == 0) return;
	if (! TableOfReal_equalLabels (me, thee, 1, 1))
	{
		(void) Melder_error1 (L"Proximity_Distance_drawScatterDiagram: Dimensions and labels must be the same.");
		return;
	}
	if (xmax <= xmin)
	{
		xmin = xmax = x[1][2];
		for (i=1; i <= thy numberOfRows-1; i++)
		{
			for (j=i+1; j <= thy numberOfColumns; j++)
			{
				if (x[i][j] > xmax) xmax = x[i][j];
				else if (x[i][j] < xmin) xmin = x[i][j];
			}
		}
	}	
	if (ymax <= ymin)
	{
		ymin = ymax = y[1][2];
		for (i=1; i <= my numberOfRows -1; i++)
		{
			for (j=i+1; j <= my numberOfColumns; j++)
			{
				if (y[i][j] > ymax) ymax = y[i][j];
				else if (y[i][j] < ymin) ymin = y[i][j];
			}
		}
	}
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
	Graphics_setInner (g);
	for (i=1; i <= thy numberOfRows-1; i++)
	{
		for (j=i+1; j <= thy numberOfColumns; j++)
		{
			if (x[i][j] >= xmin && x[i][j] <= xmax &&
				y[i][j] >= ymin && y[i][j] <= ymax)
			{
				Graphics_mark (g, x[i][j], y[i][j], size_mm, mark);
			}
		}
	}

	Graphics_unsetInner (g);
	if (garnish)
	{
		Graphics_drawInnerBox (g);
	   	Graphics_textLeft (g, 1, L"Distance");
	   	Graphics_textBottom (g, 1, L"Dissimilarity");
		Graphics_marksBottom (g, 2, 1, 1, 0);
    	Graphics_marksLeft (g, 2, 1, 1, 0);
	}
}

void Distance_and_Configuration_drawScatterDiagram (Distance me, 
	Configuration him, Graphics g, double xmin, double xmax, double ymin, 
	double ymax, double size_mm, const wchar_t *mark, int garnish)
{
	Distance dist = Configuration_to_Distance (him);
	 
	if (! dist) return;
	
	Proximity_Distance_drawScatterDiagram (me, dist, g, xmin, xmax, ymin, 
		ymax, size_mm, mark, garnish);
	forget (dist);
}

FORM (Distance_to_ScalarProduct, L"Distance: To ScalarProduct", L"Distance: To ScalarProduct...")
	BOOLEAN (L"Make sum of squares equal 1.0", 1)
	OK
DO
	EVERY_TO (Distance_to_ScalarProduct ((structDistance *)OBJECT,
		GET_INTEGER (L"Make sum of squares equal 1.0")))
END

DIRECT (Distance_to_Dissimilarity)
	EVERY_TO (Distance_to_Dissimilarity ((structDistance *)OBJECT))
END

FORM (Distances_indscal, L"Distance: To Configuration (indscal)", L"Distance: To Configuration (indscal)...")
	NATURAL (L"Number of dimensions", L"2")
	BOOLEAN (L"Normalize scalar products", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Minimization parameters")
	REAL (L"Tolerance", L"1e-5")
	NATURAL (L"Maximum number of iterations", L"100 (= each repetition)")
	NATURAL (L"Number of repetitions", L"1")
	OK
DO
	Distances distances = Distances_create();
	Configuration cresult; Salience wresult;
	WHERE (SELECTED) (void) Collection_addItem (distances, OBJECT);
	Distances_indscal (distances, GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Normalize scalar products"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Maximum number of iterations"),
		GET_INTEGER (L"Number of repetitions"),
		1, &cresult, &wresult);
	distances -> size = 0; forget (distances);
	if (! praat_new1 (cresult, L"indscal") ||
		! praat_new1 (wresult, L"indscal")) return 0;
END

FORM (Distance_and_Configuration_drawScatterDiagram, L"Distance & Configuration: Draw scatter diagram",
	L"Distance & Configuration: Draw scatter diagram...")
	REAL (L"Minimum x-distance", L"0.0")
	REAL (L"Maximum x-distance", L"0.0")
	REAL (L"Minimum y-distance", L"0.0")
	REAL (L"Maximum y-distance", L"0.0")
	POSITIVE (L"Mark size (mm)", L"1.0")
	SENTENCE (L"Mark string (+xo.)", L"+")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	praat_picture_open ();
	Distance_and_Configuration_drawScatterDiagram ((structDistance *)ONLY (classDistance),
		(structConfiguration *)ONLY (classConfiguration), GRAPHICS, GET_REAL (L"Minimum x-distance"),
		GET_REAL (L"Maximum x-distance"),
		GET_REAL (L"Minimum y-distance"), GET_REAL (L"Maximum y-distance"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"),
		GET_INTEGER (L"Garnish"));
	praat_picture_close ();
END

FORM (Distance_Configuration_indscal, L"Distance & Configuration: To Configuration (indscal)",
	L"Distance & Configuration: To Configuration (indscal)...")
	BOOLEAN (L"Normalize scalar products", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Minimization parameters")
	REAL (L"Tolerance", L"1e-5")
	NATURAL (L"Maximum number of iterations", L"100 (= each repetition)")
	OK
DO
	Distances distances = Distances_create();
	Configuration cresult; Salience wresult;
	Configuration configuration = (structConfiguration *)ONLY (classConfiguration);
	WHERE (SELECTED)
	{
		if (CLASS == classDistance)
		{
			(void) Collection_addItem (distances, OBJECT);
		}
	}
	Distances_Configuration_indscal (distances, configuration,
		GET_INTEGER (L"Normalize scalar products"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Maximum number of iterations"), 1, &cresult, &wresult);
	distances -> size = 0; forget (distances);
	if (! praat_new1 (cresult, L"indscal") ||
		! praat_new1 (wresult, L"indscal")) return 0;
END

FORM (Distance_Configuration_vaf, L"Distance & Configuration: Get VAF", L"Distance & Configuration: Get VAF...")
	BOOLEAN (L"Normalize scalar products", 1)
	OK
DO
	Distances distances = Distances_create();
	double vaf;
	Configuration configuration = (structConfiguration *)ONLY (classConfiguration);
	WHERE (SELECTED)
	{
		if (CLASS == classDistance)
		{
			(void) Collection_addItem (distances, OBJECT);
		}
	}
	Distances_Configuration_vaf (distances, configuration,
		GET_INTEGER (L"Normalize scalar products"), &vaf);
	distances -> size = 0;
	forget (distances);
	Melder_information1 (Melder_double (vaf));
END

FORM (Distance_Configuration_Salience_vaf, L"Distance & Configuration & Salience: Get VAF", L"Distance & Configuration & Salience: Get VAF...")
	BOOLEAN (L"Normalize scalar products", 1)
	OK
DO
	Distances distances = Distances_create();
	double vaf;
	WHERE (SELECTED)
	{
		if (CLASS == classDistance)
		{
			(void) Collection_addItem (distances, OBJECT);
		}
	}
	Distances_Configuration_Salience_vaf (distances, (structConfiguration *)ONLY (classConfiguration),
		(structSalience *)ONLY (classSalience), GET_INTEGER (L"Normalize scalar products"), &vaf);
	Melder_information1 (Melder_double (vaf));
	distances -> size = 0;
	forget (distances);
END

FORM (Dissimilarity_Configuration_Salience_vaf, L"Dissimilarity & Configuration & Salience: Get VAF",
	L"Dissimilarity & Configuration & Salience: Get VAF...")
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	BOOLEAN (L"Normalize scalar products", 1)
	OK
DO
	Dissimilarities dissimilarities = Dissimilarities_create();
	double vaf;
	WHERE (SELECTED)
	{
		if (CLASS == classDissimilarity)
		{
			(void) Collection_addItem (dissimilarities, OBJECT);
		}
	}
	Dissimilarities_Configuration_Salience_vaf (dissimilarities,
		(structConfiguration *)ONLY (classConfiguration),
		(structSalience *)ONLY (classSalience), GET_INTEGER (L"Handling of ties"),
		GET_INTEGER (L"Normalize scalar products"), &vaf);
	Melder_information1 (Melder_double (vaf));
	dissimilarities -> size = 0;
	forget (dissimilarities);
END

FORM (Distance_Configuration_Salience_indscal,
	L"Distance & Configuration & Salience: To Configuration (indscal)",
	L"Distance & Configuration & Salience: To Configuration (indscal)...")
	BOOLEAN (L"Normalize scalar products", 1)
	LABEL (L"ui/editors/AmplitudeTierEditor.h", L"Minimization parameters")
	REAL (L"Tolerance", L"1e-5")
	NATURAL (L"Maximum number of iterations", L"100")
	OK
DO
	Distances distances = Distances_create();
	int showprocess = 1;
	double vaf;
	Configuration cresult;
	Salience wresult;
	WHERE (SELECTED)
	{
		if (CLASS == classDistance)
		{
			(void) Collection_addItem (distances, OBJECT);
		}
	}
	Distances_Configuration_Salience_indscal (distances,
		(structConfiguration *)ONLY (classConfiguration), (structSalience *)ONLY (classSalience),
		GET_INTEGER (L"Normalize scalar products"), GET_REAL (L"Tolerance"),
		GET_INTEGER (L"Maximum number of iterations"), showprocess, &cresult,
		&wresult, &vaf);
	distances -> size = 0;
	forget (distances);
	if (! praat_new1 (cresult, L"indscal") ||
		! praat_new1 (wresult, L"indscal")) return 0;
END


FORM (Distances_to_Configuration_ytl, L"Distance: To Configuration (ytl)", L"Distance: To Configuration (ytl)...")
	NATURAL (L"Number of dimensions", L"2")
	BOOLEAN (L"Normalize scalar products", 1)
	BOOLEAN (L"Salience object", 0)
	OK
DO
	Distances me = Distances_create();
	Configuration cresult; Salience wresult;
	WHERE (SELECTED) (void) Collection_addItem (me, OBJECT);
	Distances_to_Configuration_ytl (me, GET_INTEGER (L"Number of dimensions"),
		GET_INTEGER (L"Normalize scalar products"), &cresult, &wresult);
	my size = 0;
	forget (me);
	if (! praat_new1 (cresult, NULL)) return 0;
	if (GET_INTEGER (L"Salience object"))
	{
		if (! praat_new1 (wresult, NULL)) return 0;
	}
	else forget (wresult);
END

FORM (Dissimilarity_Distance_monotoneRegression, L"Dissimilarity & Distance: Monotone regression", 0)
	RADIO (L"Handling of ties", 1)
	RADIOBUTTON (L"Primary approach")
	RADIOBUTTON (L"Secondary approach")
	OK
DO
	Dissimilarity dissimilarity = (structDissimilarity *)ONLY (classDissimilarity);
	if (! praat_new1 (Dissimilarity_Distance_monotoneRegression (dissimilarity,
		(structDistance *)ONLY (classDistance),
		GET_INTEGER (L"Handling of ties")), dissimilarity -> name)) return 0;
END

FORM (Distance_Dissimilarity_drawShepardDiagram, L"Distance & Dissimilarity: Draw Shepard diagram", L"ui/editors/AmplitudeTierEditor.h")
	REAL (L"Minimum dissimilarity", L"0.0")
	REAL (L"Maximum dissimilarity", L"0.0")
	REAL (L"left Distance range", L"0.0")
	REAL (L"right Distance range", L"0.0")
	POSITIVE (L"Mark size (mm)", L"1.0")
	SENTENCE (L"Mark string (+xo.)", L"+")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Proximity_Distance_drawScatterDiagram
		(ONLY (classDissimilarity), (structDistance *)ONLY (classDistance), GRAPHICS,
		GET_REAL (L"Minimum dissimilarity"), GET_REAL (L"Maximum dissimilarity"),
		GET_REAL (L"left Distance range"), GET_REAL (L"right Distance range"),
		GET_REAL (L"Mark size"), GET_STRING (L"Mark string"),
		GET_INTEGER (L"Garnish")))
END

DIRECT (MDS_help)
	Melder_help (L"Multidimensional scaling");
END


/************************* Salience ***************************************/

void Salience_draw (Salience me, Graphics g, int ix, int iy, int garnish)
{
	long i, j, nc2, nc1 = ix < iy ? (nc2 = iy, ix) : (nc2 = ix, iy);
	double xmin = 0, xmax = 1, ymin = 0, ymax = 1, wmax = 1;
	
	if (ix < 1 || ix > my numberOfColumns || 
		iy < 1 || iy > my numberOfColumns) return;
		
	for (i = 1; i <= my numberOfRows; i++)
	{
		for (j = nc1; j <= nc2; j++)
		{
			if (my data[i][j] > wmax) wmax = my data[i][j];
		}
	}
	xmax = ymax = wmax;
	Graphics_setInner (g);
	Graphics_setWindow (g, xmin, xmax, ymin, ymax);
    Graphics_setTextAlignment (g, Graphics_CENTRE, Graphics_HALF);
	
	for (i = 1; i <= my numberOfRows; i++)
	{
		if (my rowLabels[i])
		{
			Graphics_text (g, my data[i][ix], my data[i][iy], my rowLabels[i]);
		}
	}
	
	Graphics_setTextAlignment (g, Graphics_LEFT, Graphics_BOTTOM);
	Graphics_line (g, xmin, ymax, xmin, ymin);
	Graphics_line (g, xmin, ymin, xmax, ymin);
	/* Graphics_arc (g, xmin, ymin, xmax - xmin, 0, 90); */
    Graphics_unsetInner (g);
	
	if (garnish)
	{
		if (my columnLabels[ix])
		{
			Graphics_textBottom (g, 0, my columnLabels[ix]);
		}
		if (my columnLabels[iy])
		{
			Graphics_textLeft (g, 0, my columnLabels[iy]);
		}
	}
}

FORM (Salience_draw, L"Salience: Draw", 0)
	NATURAL (L"Horizontal dimension", L"1")
	NATURAL (L"Vertical dimension", L"2")
	BOOLEAN (L"Garnish", 1)
	OK
DO
	EVERY_DRAW (Salience_draw ((structSalience *)OBJECT, GRAPHICS,
		GET_INTEGER (L"Horizontal dimension"), GET_INTEGER (L"Vertical dimension"),
		GET_INTEGER (L"Garnish")))
END

/************************* COVARIANCE & CONFIGURATION  ********************/

FORM (Covariance_to_Configuration, L"Covariance: To Configuration", 0)
	NATURAL (L"Number of dimensions", L"2")
	OK
DO
	EVERY_TO (Covariance_to_Configuration ((structCovariance *)OBJECT,
		GET_INTEGER (L"Number of dimensions")))
END

/********* Procrustes ***************************/

DIRECT (Procrustes_help)
	Melder_help (L"Procrustes");
END

DIRECT (Procrustes_getScale)
	Procrustes me = (structProcrustes *)ONLY_OBJECT;
	Melder_information1 (Melder_double (my s));
END

/********* Casts from & to TableOfReal ***************************/

DIRECT (TableOfReal_to_Dissimilarity)
	EVERY_TO (TableOfReal_to_Dissimilarity (OBJECT))
END

DIRECT (TableOfReal_to_Similarity)
	EVERY_TO (TableOfReal_to_Similarity (OBJECT))
END

DIRECT (TableOfReal_to_Distance)
	EVERY_TO (TableOfReal_to_Distance (OBJECT))
END

DIRECT (TableOfReal_to_Salience)
	EVERY_TO (TableOfReal_to_Salience (OBJECT))
END

DIRECT (TableOfReal_to_Weight)
	EVERY_TO (TableOfReal_to_Weight (OBJECT))
END

DIRECT (TableOfReal_to_ScalarProduct)
	EVERY_TO (TableOfReal_to_ScalarProduct (OBJECT))
END

DIRECT (TableOfReal_to_Configuration)
	EVERY_TO (TableOfReal_to_Configuration (OBJECT))
END

DIRECT (TableOfReal_to_ContingencyTable)
EVERY_TO (TableOfReal_to_ContingencyTable (OBJECT))
END

/********************** TableOfReal ***************************************/

DIRECT (TableOfReal_getTableNorm)
	Melder_information1 (Melder_double (TableOfReal_getTableNorm (ONLY_OBJECT)));
END

FORM (TableOfReal_normalizeTable, L"TableOfReal: Normalize table", L"TableOfReal: Normalize table...")
	POSITIVE (L"Norm", L"1.0")
	OK
DO
	EVERY (TableOfReal_normalizeTable (OBJECT, GET_REAL (L"Norm")))
END

FORM (TableOfReal_normalizeRows, L"TableOfReal: Normalize rows", L"TableOfReal: Normalize rows...")
	POSITIVE (L"Norm", L"1.0")
	OK
DO
	EVERY (TableOfReal_normalizeRows (OBJECT, GET_REAL (L"Norm")))
END

FORM (TableOfReal_normalizeColumns, L"TableOfReal: Normalize columns", L"TableOfReal: Normalize columns...")
	POSITIVE (L"Norm", L"1.0")
	OK
DO
	EVERY (TableOfReal_normalizeColumns (OBJECT, GET_REAL (L"Norm")))
END

DIRECT (TableOfReal_centreRows)
	EVERY (TableOfReal_centreRows (OBJECT))
END

DIRECT (TableOfReal_centreColumns)
	EVERY (TableOfReal_centreColumns (OBJECT))
END

DIRECT (TableOfReal_doubleCentre)
	EVERY (TableOfReal_doubleCentre (OBJECT))
END

DIRECT (TableOfReal_standardizeRows)
	EVERY (TableOfReal_standardizeRows (OBJECT))
END

DIRECT (TableOfReal_standardizeColumns)
	EVERY (TableOfReal_standardizeColumns (OBJECT))
END

DIRECT (TableOfReal_to_Confusion)
	EVERY_TO (TableOfReal_to_Confusion (OBJECT))
END

static void praat_AffineTransform_init (void *klas)
{
	praat_addAction1 (klas, 0, QUERY_BUTTON, 0, 0, 0);
	praat_addAction1 (klas, 1, L"Get transformation element...", QUERY_BUTTON, 1, DO_AffineTransform_getTransformationElement);
	praat_addAction1 (klas, 1, L"Get translation element...", QUERY_BUTTON, 1, DO_AffineTransform_getTranslationElement);
	praat_addAction1 (klas, 0, L"Invert", 0, 0, DO_AffineTransform_invert);
}

static void praat_Configuration_and_AffineTransform_init (void *transform)
{
	praat_addAction2 (classConfiguration, 1, transform, 1, L"To Configuration", 0, 0, DO_Configuration_and_AffineTransform_to_Configuration);
}

static void praat_TableOfReal_extras (void *klas)
{
	praat_addAction1 (klas, 1, L"-- get additional --", L"Get value...", 1, 0);
	praat_addAction1 (klas, 1, L"Get table norm", L"-- get additional --", 1, DO_TableOfReal_getTableNorm);
	praat_addAction1 (klas, 1, L"-- set additional --", L"Set column label (label)...", 1, 0);
	praat_addAction1 (klas, 1, L"Normalize rows...", L"-- set additional --", 1, DO_TableOfReal_normalizeRows);
	praat_addAction1 (klas, 1, L"Normalize columns...", L"Normalize rows...", 1, DO_TableOfReal_normalizeColumns);
	praat_addAction1 (klas, 1, L"Normalize table...", L"Normalize columns...", 1, DO_TableOfReal_normalizeTable);
	praat_addAction1 (klas, 1, L"Standardize rows", L"Normalize table...", 1, DO_TableOfReal_standardizeRows);
	praat_addAction1 (klas, 1, L"Standardize columns", L"Standardize rows", 1, DO_TableOfReal_standardizeColumns);
	praat_addAction1 (klas, 1, L"Test sorting...", L"Standardize columns", praat_DEPTH_1 + praat_HIDDEN, DO_TabelOfReal_testSorting);
}

extern "C" void praat_uvafon_MDS_init (void);
void praat_uvafon_MDS_init (void)
{
	Thing_recognizeClassesByName (classAffineTransform, classProcrustes,
		classContingencyTable, classDissimilarity,
		classSimilarity, classConfiguration, classDistance,
		classSalience, classScalarProduct, classWeight, NULL);
	Thing_recognizeClassByOtherName (classProcrustes, L"Procrustus");

    praat_addMenuCommand (L"Objects", L"New", L"Multidimensional scaling", 0, 0, 0);
	praat_addMenuCommand (L"Objects", L"New", L"MDS tutorial", 0, 1, DO_MDS_help);
    praat_addMenuCommand (L"Objects", L"New", L"-- MDS --", 0, 1, 0);
    praat_addMenuCommand (L"Objects", L"New", L"Create letter R example...", 0, 1, DO_Dissimilarity_createLetterRExample);
    praat_addMenuCommand (L"Objects", L"New", L"Create INDSCAL Carroll Wish example...", 0, 1, DO_INDSCAL_createCarrollWishExample);
    praat_addMenuCommand (L"Objects", L"New", L"Create Configuration...", 0, 1, DO_Configuration_create);
    praat_addMenuCommand (L"Objects", L"New", L"Draw splines...", 0, 1, DO_drawSplines);
    praat_addMenuCommand (L"Objects", L"New", L"Draw MDS class relations", 0, 1, DO_drawMDSClassRelations);

/****** 1 class ********************************************************/

	praat_addAction1 (classAffineTransform, 0, L"AffineTransform help", 0, 0, DO_AffineTransform_help);
	praat_AffineTransform_init (classAffineTransform);


	praat_addAction1 (classConfiguration, 0, L"Configuration help", 0, 0, DO_Configuration_help);
	praat_TableOfReal_init2 (classConfiguration);
	praat_TableOfReal_extras (classConfiguration);
	(void) praat_removeAction (classConfiguration, NULL, NULL, L"Insert column (index)...");
	(void) praat_removeAction (classConfiguration, NULL, NULL, L"Remove column (index)...");
	(void) praat_removeAction (classConfiguration, NULL, NULL, L"Append");
	praat_addAction1 (classConfiguration, 0, L"Draw...", DRAW_BUTTON, 1, DO_Configuration_draw);
	praat_addAction1 (classConfiguration, 0, L"Draw sigma ellipses...", L"Draw...", 1, DO_Configuration_drawSigmaEllipses);
	praat_addAction1 (classConfiguration, 0, L"Draw one sigma ellipse...", L"Draw...", 1, DO_Configuration_drawOneSigmaEllipse);
	praat_addAction1 (classConfiguration, 0, L"Draw confidence ellipses...", L"Draw sigma ellipses...", 1, DO_Configuration_drawConfidenceEllipses);
	praat_addAction1 (classConfiguration, 0, L"Draw one confidence ellipse...", L"Draw sigma ellipses...", 1, DO_Configuration_drawOneConfidenceEllipse);

		praat_addAction1 (classConfiguration, 0, L"Randomize", L"Normalize table...", 1, DO_Configuration_randomize);
		praat_addAction1 (classConfiguration, 0, L"Normalize...", L"Randomize", 1, DO_Configuration_normalize);
		praat_addAction1 (classConfiguration, 0, L"Centralize", L"Randomize", 1, DO_Configuration_centralize);
		praat_addAction1 (classConfiguration, 1, L"-- set rotations & reflections --", L"Centralize", 1, 0);

		praat_addAction1 (classConfiguration, 0, L"Rotate...", L"-- set rotations & reflections --", 1, DO_Configuration_rotate);
		praat_addAction1 (classConfiguration, 0, L"Rotate (pc)", L"Rotate...", 1, DO_Configuration_rotateToPrincipalDirections);
		praat_addAction1 (classConfiguration, 0, L"Invert dimension...", L"Rotate (pc)", 1, DO_Configuration_invertDimension);
	praat_addAction1 (classConfiguration, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classConfiguration, 0, L"To Distance", 0, 0, DO_Configuration_to_Distance);
	praat_addAction1 (classConfiguration, 0, L"To Configuration (varimax)...", 0, 0, DO_Configuration_varimax);
	praat_addAction1 (classConfiguration, 0, L"To Similarity (cc)", 0, 0, DO_Configuration_to_Similarity_cc);

	praat_addAction1 (classConfiguration, 0, L"Match configurations -", 0, 0, 0);
	praat_addAction1 (classConfiguration, 2, L"To Procrustes...", 0, 1, DO_Configurations_to_Procrustes);
	praat_addAction1 (classConfiguration, 2, L"To AffineTransform (congruence)...", 0, 1, DO_Configurations_to_AffineTransform_congruence);

	praat_addAction1 (classConfusion, 0, L"To ContingencyTable", L"To Matrix", 0, DO_Confusion_to_ContingencyTable);
	praat_addAction1 (classConfusion, 0, L"To Proximity -", L"Analyse", 0, 0);
		praat_addAction1 (classConfusion, 0, L"To Dissimilarity (pdf)...", L"To Proximity -", 1, DO_Confusion_to_Dissimilarity_pdf);
		praat_addAction1 (classConfusion, 0, L"To Similarity...", L"To Proximity -", 1, DO_Confusion_to_Similarity);
		praat_addAction1 (classConfusion, 0, L"Sum", L"Synthesize -", 1, DO_Confusions_sum);


	praat_TableOfReal_init2 (classContingencyTable);
	praat_addAction1 (classContingencyTable, 1, L"-- statistics --", L"Get value...", 1, 0);
	praat_addAction1 (classContingencyTable, 1, L"Get chi squared probability", L"-- statistics --", 1, DO_ContingencyTable_chisqProbability);
	praat_addAction1 (classContingencyTable, 1, L"Get Cramer's statistic", L"Get chi squared probability", 1, DO_ContingencyTable_cramersStatistic);
	praat_addAction1 (classContingencyTable, 1, L"Get contingency coefficient", L"Get Cramer's statistic", 1,
		DO_ContingencyTable_contingencyCoefficient);
	praat_addAction1 (classContingencyTable, 0, L"Analyse", 0, 0, 0);
	praat_addAction1 (classContingencyTable, 1, L"To Configuration (ca)...", 0, 0, DO_ContingencyTable_to_Configuration_ca);


	praat_addAction1 (classCorrelation, 0, L"To Configuration...", 0, 0, DO_Correlation_to_Configuration);

	praat_addAction1 (classDissimilarity, 0, L"Dissimilarity help", 0, 0, DO_Dissimilarity_help);
	praat_TableOfReal_init2 (classDissimilarity);
	praat_TableOfReal_extras (classDissimilarity);
	praat_addAction1 (classDissimilarity, 0, L"Get additive constant", L"Get table norm", 1, DO_Dissimilarity_getAdditiveConstant);
	praat_addAction1 (classDissimilarity, 0, CONFIGURATION_BUTTON, 0, 0, 0);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (monotone mds)...", 0, 1, DO_Dissimilarity_monotone_mds);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (i-spline mds)...", 0, 1, DO_Dissimilarity_ispline_mds);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (interval mds)...", 0, 1, DO_Dissimilarity_interval_mds);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (ratio mds)...", 0, 1, DO_Dissimilarity_ratio_mds);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (absolute mds)...", 0, 1, DO_Dissimilarity_absolute_mds);
		praat_addAction1 (classDissimilarity, 1, L"To Configuration (kruskal)...", 0, 1, DO_Dissimilarity_kruskal);
	praat_addAction1 (classDissimilarity, 0, L"To Distance...", 0, 0, DO_Dissimilarity_to_Distance);
	praat_addAction1 (classDissimilarity, 0, L"To Weight", 0, 0, DO_Dissimilarity_to_Weight);


	praat_addAction1 (classCovariance, 0, L"To Configuration...", 0, 0, DO_Covariance_to_Configuration);


	praat_TableOfReal_init2 (classDistance);
	praat_TableOfReal_extras (classDistance);
	praat_addAction1 (classDistance, 0, L"Analyse -", 0, 0, 0);
	praat_addAction1 (classDistance, 0, CONFIGURATION_BUTTON, 0, 0, 0);
		praat_addAction1 (classDistance, 0, L"To Configuration (indscal)...", 0, 1, DO_Distances_indscal);
		praat_addAction1 (classDistance, 0, L"-- linear scaling --", 0, 1, 0);
		praat_addAction1 (classDistance, 0, L"To Configuration (ytl)...", 0, 1, DO_Distances_to_Configuration_ytl);
	praat_addAction1 (classDistance, 0, L"To Dissimilarity", 0, 0, DO_Distance_to_Dissimilarity);
	praat_addAction1 (classDistance, 0, L"To ScalarProduct...", 0, 0, DO_Distance_to_ScalarProduct);


	praat_addAction1 (classProcrustes, 0, L"Procrustes help", 0, 0, DO_Procrustes_help);
	praat_AffineTransform_init (classProcrustes);
	praat_addAction1 (classProcrustes, 1, L"Get scale", QUERY_BUTTON, 1, DO_Procrustes_getScale);
	praat_addAction1 (classProcrustes, 0, L"Extract transformation matrix", 0, 0, DO_AffineTransform_extractMatrix);
	praat_addAction1 (classProcrustes, 0, L"Extract translation vector", 0, 0, DO_AffineTransform_extractTranslationVector);

	praat_TableOfReal_init2 (classSalience);
	praat_TableOfReal_extras (classSalience);
	praat_addAction1 (classSalience, 0, L"Draw...", DRAW_BUTTON, 1, DO_Salience_draw);


	praat_addAction1 (classSimilarity, 0, L"Similarity help", 0, 0, DO_Similarity_help);
	praat_TableOfReal_init2 (classSimilarity);
	praat_TableOfReal_extras (classSimilarity);
	praat_addAction1 (classSimilarity, 0, L"Analyse -", 0, 0, 0);
	praat_addAction1 (classSimilarity, 0, L"To Dissimilarity...", 0, 0, DO_Similarity_to_Dissimilarity);


	praat_TableOfReal_init2 (classScalarProduct);
	praat_TableOfReal_extras (classScalarProduct);

	praat_TableOfReal_extras (classTableOfReal);
	praat_addAction1 (classTableOfReal, 1, L"Centre rows", L"Normalize table...", 1, DO_TableOfReal_centreRows);
	praat_addAction1 (classTableOfReal, 1, L"Centre columns", L"Centre rows", 1, DO_TableOfReal_centreColumns);
	praat_addAction1 (classTableOfReal, 1, L"Double centre", L"Centre columns", 1, DO_TableOfReal_doubleCentre);
	praat_addAction1 (classTableOfReal, 0, L"Cast -", 0, 0, 0);
		praat_addAction1 (classTableOfReal, 0, L"To Confusion", 0, 1, DO_TableOfReal_to_Confusion);
		praat_addAction1 (classTableOfReal, 0, L"To Dissimilarity", 0, 1, DO_TableOfReal_to_Dissimilarity);
		praat_addAction1 (classTableOfReal, 0, L"To Similarity", 0, 1, DO_TableOfReal_to_Similarity);
		praat_addAction1 (classTableOfReal, 0, L"To Distance", 0, 1, DO_TableOfReal_to_Distance);
		praat_addAction1 (classTableOfReal, 0, L"To Salience", 0, 1, DO_TableOfReal_to_Salience);
		praat_addAction1 (classTableOfReal, 0, L"To Weight", 0, 1, DO_TableOfReal_to_Weight);
		praat_addAction1 (classTableOfReal, 0, L"To ScalarProduct", 0, 1, DO_TableOfReal_to_ScalarProduct);
		praat_addAction1 (classTableOfReal, 0, L"To Configuration", 0, 1, DO_TableOfReal_to_Configuration);
		praat_addAction1 (classTableOfReal, 0, L"To ContingencyTable", 0, 1, DO_TableOfReal_to_ContingencyTable);

	praat_TableOfReal_init2 (classWeight);


/****** 2 classes ********************************************************/

	praat_Configuration_and_AffineTransform_init (classAffineTransform);
	praat_Configuration_and_AffineTransform_init (classProcrustes);

	praat_addAction2 (classConfiguration, 0, classWeight, 1, L"Analyse", 0, 0, 0);
	praat_addAction2 (classConfiguration, 0, classWeight, 1, L"To Similarity (cc)", 0, 0, DO_Configuration_Weight_to_Similarity_cc);

	praat_addAction2 (classDissimilarity, 1, classWeight, 1, ANALYSE_BUTTON, 0, 0, 0);
	praat_addAction2 (classDissimilarity, 1, classWeight, 1, L"To Configuration (monotone mds)...", 0, 1, DO_Dissimilarity_Weight_monotone_mds);
	praat_addAction2 (classDissimilarity, 1, classWeight, 1, L"To Configuration (i-spline mds)...", 0, 1, DO_Dissimilarity_Weight_ispline_mds);
	praat_addAction2 (classDissimilarity, 1, classWeight, 1, L"To Configuration (interval mds)...", 0, 1, DO_Dissimilarity_Weight_interval_mds);
	praat_addAction2 (classDissimilarity, 1, classWeight, 1, L"To Configuration (ratio mds)...", 0, 1, DO_Dissimilarity_Weight_ratio_mds);
	praat_addAction2 (classDissimilarity, 1, classWeight, 1, L"To Configuration (absolute mds)...", 0, 1, DO_Dissimilarity_Weight_absolute_mds);


	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, DRAW_BUTTON, 0, 0, 0);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw Shepard diagram...", 0, 1, DO_Dissimilarity_Configuration_drawShepardDiagram);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"-- draw regressions --", 0, 1, 0);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw monotone regression...", 0, 1, DO_Dissimilarity_Configuration_drawMonotoneRegression);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw i-spline regression...", 0, 1, DO_Dissimilarity_Configuration_drawISplineRegression);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw interval regression...", 0, 1, DO_Dissimilarity_Configuration_drawIntervalRegression);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw ratio regression...", 0, 1, DO_Dissimilarity_Configuration_drawRatioRegression);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Draw absolute regression...", 0, 1, DO_Dissimilarity_Configuration_drawAbsoluteRegression);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, QUERY_BUTTON, 0, 0, 0);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress...", 0, 1, DO_Dissimilarity_Configuration_getStress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress (monotone mds)...", 0, 1, DO_Dissimilarity_Configuration_monotone_stress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress (i-spline mds)...", 0, 1, DO_Dissimilarity_Configuration_ispline_stress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress (interval mds)...", 0, 1, DO_Dissimilarity_Configuration_interval_stress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress (ratio mds)...", 0, 1, DO_Dissimilarity_Configuration_ratio_stress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"Get stress (absolute mds)...", 0, 1, DO_Dissimilarity_Configuration_absolute_stress);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, ANALYSE_BUTTON, 0, 0, 0);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (monotone mds)...", 0, 1, DO_Dissimilarity_Configuration_monotone_mds);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (i-spline mds)...", 0, 1, DO_Dissimilarity_Configuration_ispline_mds);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (interval mds)...", 0, 1, DO_Dissimilarity_Configuration_interval_mds);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (ratio mds)...", 0, 1, DO_Dissimilarity_Configuration_ratio_mds);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (absolute mds)...", 0, 1, DO_Dissimilarity_Configuration_absolute_mds);
	praat_addAction2 (classDissimilarity, 1, classConfiguration, 1, L"To Configuration (kruskal)...", 0, 1, DO_Dissimilarity_Configuration_kruskal);

	praat_addAction2 (classDistance, 1, classConfiguration, 1, DRAW_BUTTON, 0, 0, 0);
	praat_addAction2 (classDistance, 1, classConfiguration, 1, L"Draw scatter diagram...", 0, 0, DO_Distance_and_Configuration_drawScatterDiagram);
	praat_addAction2 (classDistance, 1, classConfiguration, 1, QUERY_BUTTON, 0, 0, 0);
	praat_addAction2 (classDistance, 0, classConfiguration, 1, L"Get VAF...", 0, 0, DO_Distance_Configuration_vaf);
	praat_addAction2 (classDistance, 1, classConfiguration, 1, ANALYSE_BUTTON, 0, 0, 0);
	praat_addAction2 (classDistance, 0, classConfiguration, 1, L"To Configuration (indscal)...", 0, 1, DO_Distance_Configuration_indscal);

	praat_addAction2 (classDistance, 1, classDissimilarity, 1, L"Draw Shepard diagram...", 0, 0, DO_Distance_Dissimilarity_drawShepardDiagram);
	praat_addAction2 (classDissimilarity, 1, classDistance, 1, L"Monotone regression...", 0, 0, DO_Dissimilarity_Distance_monotoneRegression);

/****** 3 classes ********************************************************/


	praat_addAction3 (classDissimilarity, 0, classConfiguration, 1, classSalience, 1, QUERY_BUTTON, 0, 0, 0);
	praat_addAction3 (classDissimilarity, 0, classConfiguration, 1, classSalience, 1, L"Get VAF...", 0, 1, DO_Dissimilarity_Configuration_Salience_vaf);

	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, QUERY_BUTTON, 0, 0, 0);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"Get stress (monotone mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_monotone_stress);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"Get stress (i-spline mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_ispline_stress);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"Get stress (interval mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_interval_stress);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"Get stress (ratio mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_ratio_stress);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"Get stress (absolute mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_absolute_stress);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, ANALYSE_BUTTON, 0, 0, 0);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"To Configuration (monotone mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_monotone_mds);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"To Configuration (i-spline mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_ispline_mds);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"To Configuration (interval mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_interval_mds);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"To Configuration (ratio mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_ratio_mds);
	praat_addAction3 (classDissimilarity, 1, classConfiguration, 1, classWeight, 1, L"To Configuration (absolute mds)...", 0, 1, DO_Dissimilarity_Configuration_Weight_absolute_mds);


	praat_addAction3 (classDistance, 0, classConfiguration, 1, classSalience, 1, QUERY_BUTTON, 0, 0, 0);
	praat_addAction3 (classDistance, 0, classConfiguration, 1, classSalience, 1, L"Get VAF...", 0, 1, DO_Distance_Configuration_Salience_vaf);
	praat_addAction3 (classDistance, 0, classConfiguration, 1, classSalience, 1, L"Analyse", 0, 0, 0);
	praat_addAction3 (classDistance, 0, classConfiguration, 1, classSalience, 1, L"To Configuration (indscal)...", 0, 0, DO_Distance_Configuration_Salience_indscal);
}

/* End of file praat_MDS_init.c */
