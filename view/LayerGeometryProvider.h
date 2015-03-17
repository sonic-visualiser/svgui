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

#ifndef LAYER_GEOMETRY_PROVIDER_H
#define LAYER_GEOMETRY_PROVIDER_H

#include "base/BaseTypes.h"

class ViewManager;
class Layer;

class LayerGeometryProvider
{
public:
    /**
     * Retrieve the first visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.  The result may be negative.
     */
    virtual sv_frame_t getStartFrame() const = 0;

    /**
     * Return the centre frame of the visible widget.  This is an
     * exact value that does not depend on the zoom block size.  Other
     * frame values (start, end) are calculated from this based on the
     * zoom and other factors.
     */
    virtual sv_frame_t getCentreFrame() const = 0;

    /**
     * Retrieve the last visible sample frame on the widget.
     * This is a calculated value based on the centre-frame, widget
     * width and zoom level.
     */
    virtual sv_frame_t getEndFrame() const = 0;

    /**
     * Return the pixel x-coordinate corresponding to a given sample
     * frame (which may be negative).
     */
    virtual int getXForFrame(sv_frame_t frame) const = 0;

    /**
     * Return the closest frame to the given pixel x-coordinate.
     */
    virtual sv_frame_t getFrameForX(int x) const = 0;

    /**
     * Return the pixel y-coordinate corresponding to a given
     * frequency, if the frequency range is as specified.  This does
     * not imply any policy about layer frequency ranges, but it might
     * be useful for layers to match theirs up if desired.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    virtual double getYForFrequency(double frequency, double minFreq, double maxFreq, 
                                    bool logarithmic) const = 0;

    /**
     * Return the closest frequency to the given pixel y-coordinate,
     * if the frequency range is as specified.
     *
     * Not thread-safe in logarithmic mode.  Call only from GUI thread.
     */
    virtual double getFrequencyForY(int y, double minFreq, double maxFreq,
			   bool logarithmic) const = 0;

    /**
     * Return the zoom level, i.e. the number of frames per pixel
     */
    virtual int getZoomLevel() const = 0;

    /**
     * To be called from a layer, to obtain the extent of the surface
     * that the layer is currently painting to. This may be the extent
     * of the view (if 1x display scaling is in effect) or of a larger
     * cached pixmap (if greater display scaling is in effect).
     */
    virtual QRect getPaintRect() const = 0;

    virtual QSize getPaintSize() const { return getPaintRect().size(); }
    virtual int getPaintWidth() const { return getPaintRect().width(); }
    virtual int getPaintHeight() const { return getPaintRect().height(); }

    virtual bool hasLightBackground() const = 0;
    virtual QColor getForeground() const = 0;
    virtual QColor getBackground() const = 0;

    virtual ViewManager *getViewManager() const = 0;

    virtual bool shouldIlluminateLocalFeatures(const Layer *, QPoint &) const = 0;

    enum TextStyle {
	BoxedText,
	OutlinedText,
        OutlinedItalicText
    };

    virtual void drawVisibleText(QPainter &p, int x, int y,
				 QString text, TextStyle style) const = 0;

    virtual void drawMeasurementRect(QPainter &p, const Layer *,
                                     QRect rect, bool focus) const = 0;

    virtual QWidget *getWidget() const = 0;
};

#endif
