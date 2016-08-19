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

#include "ColourMapper.h"

#include <iostream>

#include <cmath>

#include "base/Debug.h"

#include <vector>

using namespace std;

static vector<QColor> convertStrings(const vector<QString> &strs)
{
    vector<QColor> converted;
    for (const auto &s: strs) converted.push_back(QColor(s));
    reverse(converted.begin(), converted.end());
    return converted;
}

static vector<QColor> ice = convertStrings({
        // Based on ColorBrewer ylGnBu
        "#ffffff", "#ffff00", "#f7fcf0", "#e0f3db", "#ccebc5", "#a8ddb5",
        "#7bccc4", "#4eb3d3", "#2b8cbe", "#0868ac", "#084081", "#042040"
        });

static vector<QColor> cherry = convertStrings({
        "#f7f7f7", "#fddbc7", "#f4a582", "#d6604d", "#b2182b", "#dd3497",
        "#ae017e", "#7a0177", "#49006a"
        });
    
static void
mapDiscrete(double norm, vector<QColor> &colours, double &r, double &g, double &b)
{
    int n = int(colours.size());
    double m = norm * (n-1);
    if (m >= n-1) { colours[n-1].getRgbF(&r, &g, &b, 0); return; }
    if (m <= 0) { colours[0].getRgbF(&r, &g, &b, 0); return; }
    int base(int(floor(m)));
    double prop0 = (base + 1.0) - m, prop1 = m - base;
    QColor c0(colours[base]), c1(colours[base+1]);
    r = c0.redF() * prop0 + c1.redF() * prop1;
    g = c0.greenF() * prop0 + c1.greenF() * prop1;
    b = c0.blueF() * prop0 + c1.blueF() * prop1;
}

ColourMapper::ColourMapper(int map, double min, double max) :
    m_map(map),
    m_min(min),
    m_max(max)
{
    if (m_min == m_max) {
        cerr << "WARNING: ColourMapper: min == max (== " << m_min
                  << "), adjusting" << endl;
        m_max = m_min + 1;
    }
}

ColourMapper::~ColourMapper()
{
}

int
ColourMapper::getColourMapCount()
{
    return 12;
}

QString
ColourMapper::getColourMapName(int n)
{
    if (n >= getColourMapCount()) return QObject::tr("<unknown>");
    StandardMap map = (StandardMap)n;

    switch (map) {
    case Green:            return QObject::tr("Green");
    case WhiteOnBlack:     return QObject::tr("White on Black");
    case BlackOnWhite:     return QObject::tr("Black on White");
    case Cherry:           return QObject::tr("Cherry");
    case Wasp:             return QObject::tr("Wasp");
    case Ice:              return QObject::tr("Ice");
    case Sunset:           return QObject::tr("Sunset");
    case FruitSalad:       return QObject::tr("Fruit Salad");
    case Banded:           return QObject::tr("Banded");
    case Highlight:        return QObject::tr("Highlight");
    case Printer:          return QObject::tr("Printer");
    case HighGain:         return QObject::tr("High Gain");
    }

    return QObject::tr("<unknown>");
}

QColor
ColourMapper::map(double value) const
{
    double norm = (value - m_min) / (m_max - m_min);
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    
    double h = 0.0, s = 0.0, v = 0.0, r = 0.0, g = 0.0, b = 0.0;
    bool hsv = true;

    double blue = 0.6666, pieslice = 0.3333;

    if (m_map >= getColourMapCount()) return Qt::black;
    StandardMap map = (StandardMap)m_map;

    switch (map) {

    case Green:
        h = blue - norm * 2.0 * pieslice;
        s = 0.5f + norm/2.0;
        v = norm;
        break;

    case WhiteOnBlack:
        r = g = b = norm;
        hsv = false;
        break;

    case BlackOnWhite:
        r = g = b = 1.0 - norm;
        hsv = false;
        break;

    case Cherry:
        hsv = false;
        mapDiscrete(norm, cherry, r, g, b);
        break;

    case Wasp:
        h = 0.15;
        s = 1.0;
        v = norm;
        break;

    case Sunset:
        r = (norm - 0.24) * 2.38;
        if (r > 1.0) r = 1.0;
        if (r < 0.0) r = 0.0;
        g = (norm - 0.64) * 2.777;
        if (g > 1.0) g = 1.0;
        if (g < 0.0) g = 0.0;
        b = (3.6f * norm);
        if (norm > 0.277) b = 2.0 - b;
        if (b > 1.0) b = 1.0;
        if (b < 0.0) b = 0.0;
        hsv = false;
        break;

    case FruitSalad:
        h = blue + (pieslice/6.0) - norm;
        if (h < 0.0) h += 1.0;
        s = 1.0;
        v = 1.0;
        break;

    case Banded:
        if      (norm < 0.125) return Qt::darkGreen;
        else if (norm < 0.25)  return Qt::green;
        else if (norm < 0.375) return Qt::darkBlue;
        else if (norm < 0.5)   return Qt::blue;
        else if (norm < 0.625) return Qt::darkYellow;
        else if (norm < 0.75)  return Qt::yellow;
        else if (norm < 0.875) return Qt::darkRed;
        else                   return Qt::red;
        break;

    case Highlight:
        if (norm > 0.99) return Qt::white;
        else return Qt::darkBlue;

    case Printer:
        if (norm > 0.8) {
            r = 1.0;
        } else if (norm > 0.7) {
            r = 0.9;
        } else if (norm > 0.6) {
            r = 0.8;
        } else if (norm > 0.5) {
            r = 0.7;
        } else if (norm > 0.4) {
            r = 0.6;
        } else if (norm > 0.3) {
            r = 0.5;
        } else if (norm > 0.2) {
            r = 0.4;
        } else {
            r = 0.0;
        }
        r = g = b = 1.0 - r;
        hsv = false;
        break;

    case HighGain:
        if (norm <= 1.0 / 256.0) {
            norm = 0.0;
        } else {
            norm = 0.1f + (pow(((norm - 0.5) * 2.0), 3.0) + 1.0) / 2.081;
        }
        // now as for Sunset
        r = (norm - 0.24) * 2.38;
        if (r > 1.0) r = 1.0;
        if (r < 0.0) r = 0.0;
        g = (norm - 0.64) * 2.777;
        if (g > 1.0) g = 1.0;
        if (g < 0.0) g = 0.0;
        b = (3.6f * norm);
        if (norm > 0.277) b = 2.0 - b;
        if (b > 1.0) b = 1.0;
        if (b < 0.0) b = 0.0;
        hsv = false;
/*
        if (r > 1.0) r = 1.0;
        r = g = b = 1.0 - r;
        hsv = false;
*/
        break;

    case Ice:
        hsv = false;
        mapDiscrete(norm, ice, r, g, b);
    }

    if (hsv) {
        return QColor::fromHsvF(h, s, v);
    } else {
        return QColor::fromRgbF(r, g, b);
    }
}

QColor
ColourMapper::getContrastingColour() const
{
    if (m_map >= getColourMapCount()) return Qt::white;
    StandardMap map = (StandardMap)m_map;

    switch (map) {

    case Green:
        return QColor(255, 150, 50);

    case WhiteOnBlack:
        return Qt::red;

    case BlackOnWhite:
        return Qt::darkGreen;

    case Cherry:
        return Qt::green;

    case Wasp:
        return QColor::fromHsv(240, 255, 255);

    case Ice:
        return Qt::red;

    case Sunset:
        return Qt::white;

    case FruitSalad:
        return Qt::white;

    case Banded:
        return Qt::cyan;

    case Highlight:
        return Qt::red;

    case Printer:
        return Qt::red;

    case HighGain:
        return Qt::red;
    }

    return Qt::white;
}

bool
ColourMapper::hasLightBackground() const
{
    if (m_map >= getColourMapCount()) return false;
    StandardMap map = (StandardMap)m_map;

    switch (map) {

    case BlackOnWhite:
    case Printer:
    case HighGain:
        return true;

    case Green:
    case Sunset:
    case WhiteOnBlack:
    case Cherry:
    case Wasp:
    case Ice:
    case FruitSalad:
    case Banded:
    case Highlight:
        
    default:
        return false;
    }
}


