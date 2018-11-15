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

#ifndef VIEW_PROXY_H
#define VIEW_PROXY_H

#include "layer/LayerGeometryProvider.h"

class ViewProxy : public LayerGeometryProvider
{
public:
    ViewProxy(View *view, int scaleFactor) :
        m_view(view), m_scaleFactor(scaleFactor) { }

    virtual int getId() const {
        return m_view->getId();
    }
    virtual sv_frame_t getStartFrame() const {
        return m_view->getStartFrame();
    }
    virtual sv_frame_t getCentreFrame() const {
        return m_view->getCentreFrame();
    }
    virtual sv_frame_t getEndFrame() const {
        return m_view->getEndFrame();
    }
    virtual int getXForFrame(sv_frame_t frame) const {
        //!!! not actually correct, if frame lies between view's pixels
        return m_scaleFactor * m_view->getXForFrame(frame);
    }
    virtual sv_frame_t getFrameForX(int x) const {
        sv_frame_t f0 = m_view->getFrameForX(x / m_scaleFactor);
        if (m_scaleFactor == 1) return f0;
        sv_frame_t f1 = m_view->getFrameForX((x / m_scaleFactor) + 1);
        return f0 + ((f1 - f0) * (x % m_scaleFactor)) / m_scaleFactor;
    }
    virtual int getXForViewX(int viewx) const {
        return viewx * m_scaleFactor;
    }
    virtual int getViewXForX(int x) const {
        return x / m_scaleFactor;
    }
    virtual sv_frame_t getModelsStartFrame() const {
        return m_view->getModelsStartFrame();
    }
    virtual sv_frame_t getModelsEndFrame() const {
        return m_view->getModelsEndFrame();
    }
    virtual double getYForFrequency(double frequency,
                                    double minFreq, double maxFreq, 
                                    bool logarithmic) const {
        return m_scaleFactor *
            m_view->getYForFrequency(frequency, minFreq, maxFreq, logarithmic);
    }
    virtual double getFrequencyForY(double y, double minFreq, double maxFreq,
                                    bool logarithmic) const {
        return m_view->getFrequencyForY
            (y / m_scaleFactor, minFreq, maxFreq, logarithmic);
    }
    virtual int getTextLabelHeight(const Layer *layer, QPainter &paint) const {
        return m_scaleFactor * m_view->getTextLabelHeight(layer, paint);
    }
    virtual bool getValueExtents(QString unit, double &min, double &max,
                                 bool &log) const {
        return m_view->getValueExtents(unit, min, max, log);
    }
    virtual ZoomLevel getZoomLevel() const {
        ZoomLevel z = m_view->getZoomLevel();
        //!!!
//        cerr << "getZoomLevel: from " << z << " to ";
        if (z.zone == ZoomLevel::FramesPerPixel) {
            z.level /= m_scaleFactor;
            if (z.level < 1) {
                z.level = 1;
            }
        } else {
            //!!!???
            z.level *= m_scaleFactor;
        }
//        cerr << z << endl;
        return z;
    }
    virtual QRect getPaintRect() const {
        QRect r = m_view->getPaintRect();
        return QRect(r.x() * m_scaleFactor,
                     r.y() * m_scaleFactor,
                     r.width() * m_scaleFactor,
                     r.height() * m_scaleFactor);
    }
    virtual QSize getPaintSize() const {
        return getPaintRect().size();
    }
    virtual int getPaintWidth() const { 
        return getPaintRect().width();
    }
    virtual int getPaintHeight() const { 
        return getPaintRect().height();
    }
    virtual bool hasLightBackground() const {
        return m_view->hasLightBackground();
    }
    virtual QColor getForeground() const {
        return m_view->getForeground();
    }
    virtual QColor getBackground() const {
        return m_view->getBackground();
    }
    virtual ViewManager *getViewManager() const {
        return m_view->getViewManager();
    }
        
    virtual bool shouldIlluminateLocalFeatures(const Layer *layer,
                                               QPoint &point) const {
        QPoint p;
        bool should = m_view->shouldIlluminateLocalFeatures(layer, p);
        point = QPoint(p.x() * m_scaleFactor, p.y() * m_scaleFactor);
        return should;
    }

    virtual bool shouldShowFeatureLabels() const {
        return m_view->shouldShowFeatureLabels();
    }

    virtual void drawMeasurementRect(QPainter &p, const Layer *layer,
                                     QRect rect, bool focus) const {
        m_view->drawMeasurementRect(p, layer, rect, focus);
    }

    virtual void updatePaintRect(QRect r) {
        m_view->update(r.x() / m_scaleFactor,
                       r.y() / m_scaleFactor,
                       r.width() / m_scaleFactor,
                       r.height() / m_scaleFactor);
    }

    /**
     * Scale up a size in pixels for a hi-dpi display without pixel
     * doubling. This is like ViewManager::scalePixelSize, but taking
     * and returning floating-point values rather than integer
     * pixels. It is also a little more conservative - it never
     * shrinks the size, it can only increase or leave it unchanged.
     */
    virtual double scaleSize(double size) const {
        return m_view->scaleSize(size * m_scaleFactor);
    }

    /**
     * Scale up pen width for a hi-dpi display without pixel doubling.
     * This is like scaleSize except that it also scales the
     * zero-width case.
     */
    virtual double scalePenWidth(double width) const {
        if (width <= 0) { // zero-width pen, produce a scaled one-pixel pen
            width = 1;
        }
        width *= sqrt(double(m_scaleFactor));
        return m_view->scalePenWidth(width);
    }

    /**
     * Apply scalePenWidth to a pen.
     */
    virtual QPen scalePen(QPen pen) const {
        return QPen(pen.color(), scalePenWidth(pen.width()));
    }
    
    virtual View *getView() { return m_view; }
    virtual const View *getView() const { return m_view; }

private:
    View *m_view;
    int m_scaleFactor;
};

#endif
