/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2013 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "LinearNumericalScale.h"
#include "VerticalScaleLayer.h"

#include <QPainter>

#include <cmath>

#include "view/View.h"

int
LinearNumericalScale::getWidth(View *v,
			       QPainter &paint)
{
    return paint.fontMetrics().width("-000.000");
}

void
LinearNumericalScale::paintVertical(View *v,
				    const VerticalScaleLayer *layer,
				    QPainter &paint,
				    int x0,
				    float minf,
				    float maxf)
{
    int h = v->height();

    int n = 10;

    float val = minf;
    float inc = (maxf - val) / n;

    char buffer[40];

    int w = getWidth(v, paint) + x0;

    float round = 1.f;
    int dp = 0;
    if (inc > 0) {
        int prec = trunc(log10f(inc));
        prec -= 1;
        if (prec < 0) dp = -prec;
        round = powf(10.f, prec);
#ifdef DEBUG_TIME_VALUE_LAYER
        cerr << "inc = " << inc << ", round = " << round << ", dp = " << dp << endl;
#endif
    }

    int prevy = -1;
                
    for (int i = 0; i < n; ++i) {

	int y, ty;
        bool drawText = true;

        float dispval = val;

	if (i == n-1 &&
	    v->height() < paint.fontMetrics().height() * (n*2)) {
	    if (layer->getScaleUnits() != "") drawText = false;
	}
	dispval = lrintf(val / round) * round;

#ifdef DEBUG_TIME_VALUE_LAYER
	cerr << "val = " << val << ", dispval = " << dispval << endl;
#endif

	y = layer->getYForValue(v, dispval);

	ty = y - paint.fontMetrics().height() + paint.fontMetrics().ascent() + 2;
	
	if (prevy >= 0 && (prevy - y) < paint.fontMetrics().height()) {
	    val += inc;
	    continue;
        }

	sprintf(buffer, "%.*f", dp, dispval);

	QString label = QString(buffer);

	paint.drawLine(w - 5, y, w, y);

        if (drawText) {
	    paint.drawText(w - paint.fontMetrics().width(label) - 13,
			   ty, label);
        }

        prevy = y;
	val += inc;
    }
}
