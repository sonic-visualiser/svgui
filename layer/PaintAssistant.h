/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
   
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_PAINT_ASSISTANT_H
#define SV_PAINT_ASSISTANT_H

#include <QRect>
#include <QPen>
#include <vector>

#include "base/AudioLevel.h"

class QPainter;
namespace sv {

class Layer;
class LayerGeometryProvider;

class PaintAssistant
{
public:
    enum Scale { LinearScale, MeterScale, dBScale };

    static void paintVerticalLevelScale(QPainter &p, QRect rect,
                                        double minVal, double maxVal,
                                        Scale scale, AudioLevel::Quantity sort,
                                        int &multRtn,
                                        std::vector<int> *markCoordRtns = 0);

    static int getYForValue(Scale scale, AudioLevel::Quantity sort,
                            double value, double minVal, double maxVal,
                            int minY, int height);

    enum TextStyle {
        BoxedText,
        OutlinedText,
        OutlinedItalicText
    };

    static void drawVisibleText(const LayerGeometryProvider *,
                                QPainter &p, int x, int y,
                                QString text, TextStyle style);
};

} // end namespace sv

#endif
