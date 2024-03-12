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

#ifndef COORDINATE_SCALE_H
#define COORDINATE_SCALE_H

#include <QString>

namespace sv {

class LayerGeometryProvider;

/**
 * A facility to map between coordinate and value in a given axis.
 * Queries a LayerGeometryProvider to find the proper dimensions for
 * its axis direction. A CoordinateScale object is self-contained and
 * can be passed around by value.
 */
class CoordinateScale
{
public:
    enum class FrequencyMap {
        Linear,
        Mel,
        Log
    };

    enum class Direction {
        Horizontal,
        Vertical
    };

    /**
     * Construct a linear or logarithmic scale with an arbitrary unit
     * and given extents. In the case of a log scale, the extents are
     * actual values, not log-values.
     */
    CoordinateScale(Direction direction, QString unit, bool logarithmic,
                    double minValue, double maxValue);
        
    /**
     * Construct a frequency scale with a given map and
     * extents. The extents should be in Hz and the unit of the scale
     * will be Hz.
     */
    CoordinateScale(Direction direction, FrequencyMap map,
                    double minValue, double maxValue);

    double getCoordForValue(LayerGeometryProvider *, double value) const;
    int getCoordForValueRounded(LayerGeometryProvider *, double value) const;
    double getValueForCoord(LayerGeometryProvider *, double coordinate) const;

    Direction getDirection() const { return m_direction; }
    QString getUnit() const { return m_unit; }
    double getMinValue() const { return m_minValue; }
    double getMaxValue() const { return m_maxValue; }

private:
    Direction m_direction;
    bool m_isFrequencyScale;
    QString m_unit;
    double m_logarithmic;
    FrequencyMap m_frequencyMap;
    double m_minValue;
    double m_maxValue;

    bool isLinear() const {
        if (m_isFrequencyScale) {
            return m_frequencyMap == FrequencyMap::Linear;
        } else {
            return !m_logarithmic;
        }
    }
    
    bool isLog() const {
        if (m_isFrequencyScale) {
            return m_frequencyMap == FrequencyMap::Log;
        } else {
            return m_logarithmic;
        }
    }
        
    double map(double value) const;
    double unmap(double point) const;
    void mapExtents(double &min, double &max) const;
};

} // end namespace sv

#endif

