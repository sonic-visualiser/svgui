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

#include "LayerGeometryProvider.h"

class ViewProxy : public LayerGeometryProvider
{
public:
    ViewProxy(View *view, int scaleFactor) :
	m_view(view), m_scaleFactor(scaleFactor) { }

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
	return m_scaleFactor * m_view->getXForFrame(frame);
    }
    virtual sv_frame_t getFrameForX(int x) const {
	//!!! todo: interpolate
	return m_view->getFrameForX(x / m_scaleFactor);
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
    virtual double getFrequencyForY(int y, double minFreq, double maxFreq,
				    bool logarithmic) const {
	//!!! todo: interpolate
	return m_view->getFrequencyForY(y / m_scaleFactor, minFreq, maxFreq, logarithmic);
    }
    virtual int getTextLabelHeight(const Layer *layer, QPainter &paint) const {
	return m_scaleFactor * m_view->getTextLabelHeight(layer, paint);
    }
    virtual bool getValueExtents(QString unit, double &min, double &max,
                                 bool &log) const {
	return m_view->getValueExtents(unit, min, max, log);
    }
    virtual int getZoomLevel() const {
	//!!! aarg, what if it's already 1?
	int z = m_view->getZoomLevel();
	cerr << "getZoomLevel: from " << z << " to ";
	z = z / m_scaleFactor;
	cerr << z << endl;
	return z;
    }
    virtual QRect getPaintRect() const {
	QRect r = m_view->getPaintRect();
	return QRect(r.x() * m_scaleFactor,
		     r.y() * m_scaleFactor,
		     r.width() * m_scaleFactor,
		     r.height() * m_scaleFactor);
    }
    virtual QSize getPaintSize() const { return getPaintRect().size(); }
    virtual int getPaintWidth() const { return getPaintRect().width(); }
    virtual int getPaintHeight() const { return getPaintRect().height(); }
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
	
    virtual bool shouldIlluminateLocalFeatures(const Layer *layer, QPoint &point) const {
	return m_view->shouldIlluminateLocalFeatures(layer, point);
    }
    virtual bool shouldShowFeatureLabels() const {
	return m_view->shouldShowFeatureLabels();
    }

    virtual void drawVisibleText(QPainter &p, int x, int y,
				 QString text, TextStyle style) const {
	m_view->drawVisibleText(p, x, y, text, style);
    }

    virtual void drawMeasurementRect(QPainter &p, const Layer *layer,
                                     QRect rect, bool focus) const {
	m_view->drawMeasurementRect(p, layer, rect, focus);
    }

    virtual View *getView() { return m_view; }
    virtual const View *getView() const { return m_view; }

private:
    View *m_view;
    int m_scaleFactor;
};

#endif
