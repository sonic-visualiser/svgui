/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef COLOUR_SCALE_H
#define COLOUR_SCALE_H

#include "ColourMapper.h"

/**
 * Map values within a range onto a set of colours, with a given
 * distribution (linear, log etc) and optional colourmap rotation.
 */
class ColourScale
{
public:
    enum Scale {
	LinearColourScale,
	MeterColourScale,
	LogColourScale,
	PhaseColourScale,
        PlusMinusOneScale,
        AbsoluteScale
    };

    /**
     * Create a ColourScale with the given parameters:
     * 
     * @param colourMap A colour map index as used by ColourMapper
     * @param scale Distribution for the scale
     * @param minValue Minimum value in range
     * @param maxValue Maximum value in range. Must be > minValue
     * @param threshold Threshold below which every value is mapped to
     *   background pixel 0
     * @param gain Gain to apply before clamping and mapping, typically 1
     *
     * Note that some parameters may be ignored for some scale
     * distribution settings. For example, min and max are ignored for
     * PlusMinusOneScale and PhaseColourScale and threshold and gain
     * are ignored for PhaseColourScale.
     */
    ColourScale(int colourMap,
		Scale scale,
		double minValue,
		double maxValue,
		double threshold,
		double gain);

    /**
     * Return a pixel number (in the range 0-255 inclusive)
     * corresponding to the given value.  The pixel 0 is used only for
     * values below the threshold supplied in the constructor. All
     * other values are mapped onto the range 1-255.
     */
    int getPixel(double value);

    /**
     * Return the colour for the given pixel number (which must be in
     * the range 0-255). The pixel 0 is always the background
     * colour. Other pixels are mapped taking into account the given
     * colourmap rotation (which is also a value in the range 0-255).
     */
    QColor getColourForPixel(int pixel, int rotation);
    
    /**
     * Return the colour corresponding to the given value. This is
     * equivalent to getColourForPixel(getPixel(value), rotation).
     */
    QColor getColour(double value, int rotation) {
	return getColourForPixel(getPixel(value), rotation);
    }

private:
    ColourMapper m_mapper;
    Scale m_scale;
    double m_min;
    double m_max;
    double m_mappedMin;
    double m_mappedMax;
    double m_threshold;
    double m_gain;
    static int m_maxPixel;
};

#endif
