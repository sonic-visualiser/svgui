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

#ifndef SV_COLOUR_MAPPER_H
#define SV_COLOUR_MAPPER_H

#include <QObject>
#include <QColor>
#include <QString>
#include <QPixmap>

/**
 * A class for mapping intensity values onto various colour maps.
 */
class ColourMapper
{
public:
    ColourMapper(int map, double minValue, double maxValue);
    ~ColourMapper();

    ColourMapper(const ColourMapper &) = default;
    ColourMapper &operator=(const ColourMapper &) = default;

    enum StandardMap {
        Green,
        Sunset,
        WhiteOnBlack,
        BlackOnWhite,
        Cherry,
        Wasp,
        Ice,
        FruitSalad,
        Banded,
        Highlight,
        Printer,
        HighGain
    };

    int getMap() const { return m_map; }
    double getMinValue() const { return m_min; }
    double getMaxValue() const { return m_max; }

    static int getColourMapCount();
    static QString getColourMapName(int n);

    QColor map(double value) const;

    QColor getContrastingColour() const; // for cursors etc
    bool hasLightBackground() const;

    QPixmap getExamplePixmap(QSize size) const;
    
protected:
    int m_map;
    double m_min;
    double m_max;
};

#endif

