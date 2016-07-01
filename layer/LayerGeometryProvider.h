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

#include <QMutex>
#include <QMutexLocker>
#include <QPainter>

class ViewManager;
class View;
class Layer;

class LayerGeometryProvider
{
protected:
    static int getNextId() {
        static QMutex idMutex;
        static int nextId = 1;
        static int maxId = INT_MAX;
        QMutexLocker locker(&idMutex);
        int id = nextId;
        if (nextId == maxId) {
            // we don't expect this to happen in the lifetime of a
            // process, but it would be undefined behaviour if it did
            // since we're using a signed int, so we should really
            // guard for it...
            nextId = 1;
        } else {
            nextId++;
        }
        return id;
    }            
    
public:
    LayerGeometryProvider() { }
    
    /**
     * Retrieve the id of this object.
     */
    virtual int getId() const = 0;

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

    virtual sv_frame_t getModelsStartFrame() const = 0;
    virtual sv_frame_t getModelsEndFrame() const = 0;

    /**
     * Return the closest pixel x-coordinate corresponding to a given
     * view x-coordinate.
     */
    virtual int getXForViewX(int viewx) const = 0;
    
    /**
     * Return the closest view x-coordinate corresponding to a given
     * pixel x-coordinate.
     */
    virtual int getViewXForX(int x) const = 0;
    
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

    virtual int getTextLabelHeight(const Layer *layer, QPainter &) const = 0;

    virtual bool getValueExtents(QString unit, double &min, double &max,
                                 bool &log) const = 0;

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
    virtual bool shouldShowFeatureLabels() const = 0;

    virtual void drawMeasurementRect(QPainter &p, const Layer *,
                                     QRect rect, bool focus) const = 0;

    virtual void updatePaintRect(QRect r) = 0;
    
    virtual View *getView() = 0;
    virtual const View *getView() const = 0;
};

#endif
