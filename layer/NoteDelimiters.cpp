/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "NoteDelimiters.h"

#include <QPainter>

#include <cmath>

#include "base/Pitch.h"

#include "LayerGeometryProvider.h"

#include <iostream>
using namespace std;

void NoteDelimiters::paintDelimitersVertical
    (LayerGeometryProvider *v, QPainter &paint, QRect r,
    double minf, double maxf)
{
    int x0 = r.x(), y0 = r.y(), x1 = r.x() + r.width(), y1 = r.y() + r.height();

    int py = y1;
    paint.setPen(Qt::gray);

    for (int i = 0; i < 128; ++i) {

        double f = Pitch::getFrequencyForPitch(i);
        int y = int(lrint(v->getYForFrequency(f, minf, maxf, true)));

        if (y < y0 - 2) break;
        if (y > y1 + 2) {
            continue;
        }

        paint.drawLine(x0, (y + py) / 2, x1, (y + py) / 2);

        py = y;
    }
}