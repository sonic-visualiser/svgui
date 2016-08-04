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

#include "ColourScale.h"

#include "base/AudioLevel.h"
#include "base/LogRange.h"

#include <cmath>
#include <iostream>

using namespace std;

int ColourScale::m_maxPixel = 255;

ColourScale::ColourScale(Parameters parameters) :
    m_params(parameters),
    m_mapper(m_params.colourMap, 1.f, double(m_maxPixel))
{
    if (m_params.minValue >= m_params.maxValue) {
        cerr << "ERROR: ColourScale::ColourScale: minValue = "
             << m_params.minValue << ", maxValue = " << m_params.maxValue << endl;
	throw std::logic_error("maxValue must be greater than minValue");
    }

    m_mappedMin = m_params.minValue;
    m_mappedMax = m_params.maxValue;

    if (m_mappedMin < m_params.threshold) {
        m_mappedMin = m_params.threshold;
    }
    
    if (m_params.scaleType == ColourScaleType::Log) {

	LogRange::mapRange(m_mappedMin, m_mappedMax);
	
    } else if (m_params.scaleType == ColourScaleType::PlusMinusOne) {
	
	m_mappedMin = -1.0;
	m_mappedMax =  1.0;

    } else if (m_params.scaleType == ColourScaleType::Absolute) {

	m_mappedMin = fabs(m_mappedMin);
	m_mappedMax = fabs(m_mappedMax);
	if (m_mappedMin >= m_mappedMax) {
	    std::swap(m_mappedMin, m_mappedMax);
	}
    }

    if (m_mappedMin >= m_mappedMax) {
        cerr << "ERROR: ColourScale::ColourScale: minValue = " << m_params.minValue
             << ", maxValue = " << m_params.maxValue
             << ", threshold = " << m_params.threshold
             << ", scale = " << int(m_params.scaleType)
             << " resulting in mapped minValue = " << m_mappedMin
             << ", mapped maxValue = " << m_mappedMax << endl;
	throw std::logic_error("maxValue must be greater than minValue [after mapping]");
    }
}

ColourScale::~ColourScale()
{
}

ColourScaleType
ColourScale::getScale() const
{
    return m_params.scaleType;
}

int
ColourScale::getPixel(double value) const
{
    double maxPixF = m_maxPixel;

    if (m_params.scaleType == ColourScaleType::Phase) {
	double half = (maxPixF - 1.f) / 2.f;
	return 1 + int((value * half) / M_PI + half);
    }
    
    value *= m_params.gain;

//    value = pow(value, m_params.multiple);
    
    if (value < m_params.threshold) return 0;

    double mapped = value;

    if (m_params.scaleType == ColourScaleType::Log) {
	mapped = LogRange::map(value);
    } else if (m_params.scaleType == ColourScaleType::PlusMinusOne) {
	if (mapped < -1.f) mapped = -1.f;
	if (mapped > 1.f) mapped = 1.f;
    } else if (m_params.scaleType == ColourScaleType::Absolute) {
	if (mapped < 0.f) mapped = -mapped;
    }

    mapped *= m_params.multiple;
    
    if (mapped < m_mappedMin) {
	mapped = m_mappedMin;
    }
    if (mapped > m_mappedMax) {
	mapped = m_mappedMax;
    }

    double proportion = (mapped - m_mappedMin) / (m_mappedMax - m_mappedMin);

    int pixel = 0;

    if (m_params.scaleType == ColourScaleType::Meter) {
	pixel = AudioLevel::multiplier_to_preview(proportion, m_maxPixel-1) + 1;
    } else {
	pixel = int(proportion * maxPixF) + 1;
    }

    if (pixel < 0) {
	pixel = 0;
    }
    if (pixel > m_maxPixel) {
	pixel = m_maxPixel;
    }
    return pixel;
}

QColor
ColourScale::getColourForPixel(int pixel, int rotation) const
{
    if (pixel < 0) {
	pixel = 0;
    }
    if (pixel > m_maxPixel) {
	pixel = m_maxPixel;
    }
    if (pixel == 0) {
	if (m_mapper.hasLightBackground()) {
	    return Qt::white;
	} else {
	    return Qt::black;
	}
    } else {
	int target = int(pixel) + rotation;
	while (target < 1) target += m_maxPixel;
	while (target > m_maxPixel) target -= m_maxPixel;
	return m_mapper.map(double(target));
    }
}
