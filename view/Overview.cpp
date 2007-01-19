/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Overview.h"
#include "layer/Layer.h"
#include "data/model/Model.h"
#include "base/ZoomConstraint.h"

#include <QPaintEvent>
#include <QPainter>
#include <iostream>

using std::cerr;
using std::endl;

Overview::Overview(QWidget *w) :
    View(w, false),
    m_clickedInRange(false)
{
    setObjectName(tr("Overview"));
    m_followPan = false;
    m_followZoom = false;
}

void
Overview::modelChanged(size_t startFrame, size_t endFrame)
{
    View::modelChanged(startFrame, endFrame);
}

void
Overview::modelReplaced()
{
    View::modelReplaced();
}

void
Overview::registerView(View *widget)
{
    m_widgets.insert(widget);
    update(); 
}

void
Overview::unregisterView(View *widget)
{
    m_widgets.erase(widget);
    update();
}

void
Overview::viewManagerCentreFrameChanged(void *p, unsigned long f, bool)
{
//    std::cerr << "Overview[" << this << "]::viewManagerCentreFrameChanged(" 
//	      << p << ", " << f << ")" << std::endl;

    if (p == this) return;
    if (m_widgets.find(p) != m_widgets.end()) {
	update();
    }
}

void
Overview::viewManagerZoomLevelChanged(void *p, unsigned long z, bool)
{
    if (p == this) return;
    if (m_widgets.find(p) != m_widgets.end()) {
	update();
    }
}

void
Overview::viewManagerPlaybackFrameChanged(unsigned long f)
{
    bool changed = false;

    if (getXForFrame(m_playPointerFrame) != getXForFrame(f)) changed = true;
    m_playPointerFrame = f;

    if (changed) update();
}

void
Overview::paintEvent(QPaintEvent *e)
{
    // Recalculate zoom in case the size of the widget has changed.

    size_t startFrame = getModelsStartFrame();
    size_t frameCount = getModelsEndFrame() - getModelsStartFrame();
    int zoomLevel = frameCount / width();
    if (zoomLevel < 1) zoomLevel = 1;
    zoomLevel = getZoomConstraintBlockSize(zoomLevel,
					   ZoomConstraint::RoundUp);
    if (zoomLevel != m_zoomLevel) {
	m_zoomLevel = zoomLevel;
	emit zoomLevelChanged(this, m_zoomLevel, m_followZoom);
    }
    size_t centreFrame = startFrame + m_zoomLevel * (width() / 2);
    if (centreFrame > (startFrame + getModelsEndFrame())/2) {
	centreFrame = (startFrame + getModelsEndFrame())/2;
    }
    if (centreFrame != m_centreFrame) {
	m_centreFrame = centreFrame;
	emit centreFrameChanged(this, m_centreFrame, false);
    }

    View::paintEvent(e);

    QPainter paint;
    paint.begin(this);

    QRect r(rect());

    if (e) {
	r = e->rect();
	paint.setClipRect(r);
    }

    paint.setPen(Qt::black);

    int y = 0;

    int prevx0 = -10;
    int prevx1 = -10;

    for (WidgetSet::iterator i = m_widgets.begin(); i != m_widgets.end(); ++i) {
	if (!*i) continue;

	View *w = (View *)*i;

	long f0 = w->getFrameForX(0);
	long f1 = w->getFrameForX(w->width());

	int x0 = getXForFrame(f0);
	int x1 = getXForFrame(f1);

	if (x0 != prevx0 || x1 != prevx1) {
	    y += height() / 10 + 1;
	    prevx0 = x0;
	    prevx1 = x1;
	}

	if (x1 <= x0) x1 = x0 + 1;
	
	paint.drawRect(x0, y, x1 - x0, height() - 2 * y);
    }

    paint.end();
}

void
Overview::mousePressEvent(QMouseEvent *e)
{
    m_clickPos = e->pos();
    for (WidgetSet::iterator i = m_widgets.begin(); i != m_widgets.end(); ++i) {
	if (*i) {
	    m_clickedInRange = true;
	    m_dragCentreFrame = ((View *)*i)->getCentreFrame();
	    break;
	}
    }
}

void
Overview::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_clickedInRange) {
	mouseMoveEvent(e);
    }
    m_clickedInRange = false;
}

void
Overview::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_clickedInRange) return;

    long xoff = int(e->x()) - int(m_clickPos.x());
    long frameOff = xoff * m_zoomLevel;
    
    size_t newCentreFrame = m_dragCentreFrame;
    if (frameOff > 0) {
	newCentreFrame += frameOff;
    } else if (newCentreFrame >= size_t(-frameOff)) {
	newCentreFrame += frameOff;
    } else {
	newCentreFrame = 0;
    }

    if (newCentreFrame >= getModelsEndFrame()) {
	newCentreFrame = getModelsEndFrame();
	if (newCentreFrame > 0) --newCentreFrame;
    }
    
    if (std::max(m_centreFrame, newCentreFrame) -
	std::min(m_centreFrame, newCentreFrame) > size_t(m_zoomLevel)) {
	emit centreFrameChanged(this, newCentreFrame, true);
    }
}

void
Overview::mouseDoubleClickEvent(QMouseEvent *e)
{
    long frame = getFrameForX(e->x());
    emit centreFrameChanged(this, frame, true);
}

void
Overview::enterEvent(QEvent *)
{
    emit contextHelpChanged(tr("Click and drag to navigate; double-click to jump"));
}

void
Overview::leaveEvent(QEvent *)
{
    emit contextHelpChanged("");
}


