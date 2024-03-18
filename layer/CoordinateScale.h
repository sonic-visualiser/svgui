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
 *
 * These are generally only used where a scale is monotonic through
 * the visible area of the layer. Currently layers which may have more
 * than one scale region (e.g. waveforms, whose y axis may be divided
 * into multi-channel regions) or layers whose scale occupies only a
 * part of their visible area (e.g. the x axis of spectra) are not
 * able to make use of this.
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
     * Construct a continuous linear or logarithmic scale with an
     * arbitrary unit and given extents. In the case of a log scale,
     * the extents are actual values, not log-values. The display
     * extents will be initialised to the same as the value extents.
     */
    CoordinateScale(Direction direction, QString unit, bool logarithmic,
                    double minValue, double maxValue);
        
    /**
     * Construct a frequency scale with a given map and extents. The
     * extents should be in Hz and the unit of the scale will be
     * "Hz". The display extents will be initialised to the same as
     * the value extents.
     */
    CoordinateScale(Direction direction, FrequencyMap map,
                    double minValue, double maxValue);

    /**
     * Construct an integer-valued bin scale with the given
     * extents. The extents should be in bin number (zero-based) and
     * the unit of the scale will be "bins". The display extents will
     * be initialised to the same as the value extents.
     */
    CoordinateScale(Direction direction, bool logarithmic,
                    int minBin, int maxBin);
    
    double getCoordForValue(LayerGeometryProvider *, double value) const;
    int getCoordForValueRounded(LayerGeometryProvider *, double value) const;

    double getValueForCoord(LayerGeometryProvider *, double coordinate) const;
    int getValueForCoordRounded(LayerGeometryProvider *, double coordinate) const;

    Direction getDirection() const { return m_direction; }
    
    QString getUnit() const { return m_unit; }

    double getValueMinimum() const { return m_valueMin; }
    double getValueMaximum() const { return m_valueMax; }

    double getDisplayMinimum() const { return m_displayMin; }
    double getDisplayMaximum() const { return m_displayMax; }

    CoordinateScale withValueExtents(double min, double max) const;
    CoordinateScale withDisplayExtents(double min, double max) const;
    
    /**
     * Return true if the scale is logarithmic. Note that a scale
     * could have any kind of mapping - false does not imply linear.
     */
    bool isLogarithmic() const {
        if (m_isFrequencyScale) {
            return m_frequencyMap == FrequencyMap::Log;
        } else {
            return m_logarithmic;
        }
    }

    /**
     * Return true if the scale is linear.
     */
    bool isLinear() const {
        if (m_isFrequencyScale) {
            return m_frequencyMap == FrequencyMap::Linear;
        } else {
            return !m_logarithmic;
        }
    }

private:
    Direction m_direction;
    bool m_isFrequencyScale;
    bool m_isBinScale;
    QString m_unit;
    bool m_logarithmic;
    FrequencyMap m_frequencyMap;
    double m_valueMin;
    double m_valueMax;
    double m_displayMin;
    double m_displayMax;
    
    double map(double value) const;
    double unmap(double point) const;
    void mapExtents(double &min, double &max) const;
};

} // end namespace sv

#endif

