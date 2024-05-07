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

#include "CoordinateScale.h"

#include "LayerGeometryProvider.h"

#include "base/Pitch.h"
#include "base/LogRange.h"

namespace sv {

constexpr auto melFormula = Pitch::MelFormula::OShaughnessy;

CoordinateScale::CoordinateScale(Direction direction, QString unit,
                                 bool logarithmic,
                                 double minValue, double maxValue) :
    m_direction(direction),
    m_isFrequencyScale(false), // (well, it still could be if unit is Hz)
    m_isBinScale(false),
    m_unit(unit),
    m_logarithmic(logarithmic),
    m_frequencyMap(logarithmic ? FrequencyMap::Log : FrequencyMap::Linear),
    m_valueMin(minValue),
    m_valueMax(maxValue),
    m_displayMin(minValue),
    m_displayMax(maxValue)
{ }
        
CoordinateScale::CoordinateScale(Direction direction, FrequencyMap map,
                                 double minValue, double maxValue) :
    m_direction(direction),
    m_isFrequencyScale(true),
    m_isBinScale(false),
    m_unit("Hz"),
    m_logarithmic(map == FrequencyMap::Log),
    m_frequencyMap(map),
    m_valueMin(minValue),
    m_valueMax(maxValue),
    m_displayMin(minValue),
    m_displayMax(maxValue)
{ }

CoordinateScale::CoordinateScale(Direction direction, bool logarithmic,
                                 int minBin, int maxBin) :
    m_direction(direction),
    m_isFrequencyScale(false),
    m_isBinScale(true),
    m_unit("bins"),
    m_logarithmic(logarithmic),
    m_valueMin(minBin),
    m_valueMax(maxBin),
    m_displayMin(minBin),
    m_displayMax(maxBin)
{
}

CoordinateScale
CoordinateScale::withValueExtents(double min, double max) const
{
    CoordinateScale scale(*this);
    scale.m_valueMin = min;
    scale.m_valueMax = max;
    return scale;
}

CoordinateScale
CoordinateScale::withDisplayExtents(double min, double max) const
{
    CoordinateScale scale(*this);
    scale.m_displayMin = min;
    scale.m_displayMax = max;
    return scale;
}

CoordinateScale
CoordinateScale::unionWith(const CoordinateScale &other) const
{
    CoordinateScale scale(*this);

    if (scale.m_unit == "") {
        scale.m_unit = other.m_unit;
    } else if (scale.m_unit != other.m_unit) {
        // Can't take union
        return scale;
    }

    scale.m_valueMin = std::min(scale.m_valueMin, other.m_valueMin);
    scale.m_valueMax = std::max(scale.m_valueMax, other.m_valueMax);

    scale.m_displayMin = std::min(scale.m_displayMin, other.m_displayMin);
    scale.m_displayMax = std::max(scale.m_displayMax, other.m_displayMax);

    return scale;
}

void
CoordinateScale::mapExtents(double &min, double &max) const
{
    if (isLinear()) {
        return;
    }
    
    if (isLogarithmic()) {
        if (m_isBinScale) {
            min = min + 1;
            max = max + 1;
        }
        LogRange::mapRange(min, max);
        return;
    }

    if (m_isFrequencyScale) {
        if (m_frequencyMap == FrequencyMap::Mel) {
            min = Pitch::getMelForFrequency(min, melFormula);
            max = Pitch::getMelForFrequency(max, melFormula);
            return;
        }
    }

    throw std::logic_error("CoordinateScale::mapExtents: Unreachable condition reached");
}

double
CoordinateScale::map(double value) const
{
    if (isLinear()) {
        return value;
    }

    if (isLogarithmic()) {
        if (m_isBinScale) {
            value = value + 1;
        }
        return LogRange::map(value);
    }

    if (m_isFrequencyScale) {
        if (m_frequencyMap == FrequencyMap::Mel) {
            return Pitch::getMelForFrequency(value, melFormula);
        }
    }

    throw std::logic_error("CoordinateScale::map: Unreachable condition reached");
}

double
CoordinateScale::unmap(double point) const 
{
    if (isLinear()) {
        return point;
    }

    if (isLogarithmic()) {
        double value = LogRange::unmap(point);
        if (m_isBinScale) {
            return value - 1;
        } else {
            return value;
        }
    }

    if (m_isFrequencyScale) {
        if (m_frequencyMap == FrequencyMap::Mel) {
            return Pitch::getFrequencyForMel(point, melFormula);
        }
    }

    throw std::logic_error("CoordinateScale::unmap: Unreachable condition reached");
}

double
CoordinateScale::getCoordForValue(const LayerGeometryProvider *v, double value) const
{
    double minm = m_displayMin, maxm = m_displayMax;
    mapExtents(minm, maxm);

    if (minm == maxm) {
        return 0.0;
    }

    double point = map(value);
    double coord = 0.0;

//!!! refer to Colour3DPlotLayer::getYForBin and getBinForY for
//!!! handling of bin scales

    //!!! refer to VerticalBinLayer for docs of bins
    
    if (m_direction == Direction::Vertical) {
        double h = v->getPaintHeight();
        coord = h - (h * (point - minm)) / (maxm - minm);
    } else {
        double w = v->getPaintWidth();
        coord = (w * (point - minm)) / (maxm - minm);
    }

    return coord;
}

int
CoordinateScale::getCoordForValueRounded(const LayerGeometryProvider *v, double value) const
{
    return int(floor(getCoordForValue(v, value)));
}

double
CoordinateScale::getValueForCoord(const LayerGeometryProvider *v, double coordinate) const
{
    double minm = m_displayMin, maxm = m_displayMax;
    mapExtents(minm, maxm);

    double point = 0.0;
    
    if (m_direction == Direction::Vertical) {
        double h = v->getPaintHeight();
        point = minm + ((h - coordinate) * (maxm - minm)) / h;
    } else {
        double w = v->getPaintWidth();
        point = minm + (coordinate * (maxm - minm)) / w;
    }

    double value = unmap(point);
    return value;
}

int
CoordinateScale::getValueForCoordRounded(const LayerGeometryProvider *v, double coordinate) const
{
    return int(floor(getValueForCoord(v, coordinate)));
}

bool
CoordinateScale::visualRangeMatches(const CoordinateScale &other) const
{
    if (m_direction != other.m_direction) {
        return false;
    }

    if (m_isFrequencyScale) {
        if (other.m_isFrequencyScale) {
            if (m_frequencyMap != other.m_frequencyMap) {
                return false;
            }
        } else if (m_frequencyMap == FrequencyMap::Mel) {
            return false;
        } else if (m_frequencyMap == FrequencyMap::Log) {
            if (!other.m_logarithmic) {
                return false;
            }
        } else if (other.m_logarithmic) {
            return false;
        }
    } else {
        if (other.m_isFrequencyScale) {
            return other.visualRangeMatches(*this);
        } else if (m_logarithmic != other.m_logarithmic) {
            return false;
        }
    }

    double eps = 1.0e-10;
    if (fabs(m_displayMin - other.m_displayMin) > eps) {
        return false;
    }
    if (fabs(m_displayMax - other.m_displayMax) > eps) {
        return false;
    }

    return true;
}

} // end namespace sv
