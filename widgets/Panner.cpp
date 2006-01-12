/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "Panner.h"
#include "base/Layer.h"
#include "base/Model.h"
#include "base/ZoomConstraint.h"

#include <QPaintEvent>
#include <QPainter>
#include <iostream>

using std::cerr;
using std::endl;

Panner::Panner(QWidget *w) :
    View(w, false),
    m_clickedInRange(false)
{
    setObjectName(tr("Panner"));
    m_followPan = false;
    m_followZoom = false;
}

void
Panner::modelChanged(size_t startFrame, size_t endFrame)
{
    View::modelChanged(startFrame, endFrame);
}

void
Panner::modelReplaced()
{
    View::modelReplaced();
}

void
Panner::registerView(View *widget)
{
    m_widgets[widget] = WidgetRec(0, -1);
    update(); 
}

void
Panner::unregisterView(View *widget)
{
    m_widgets.erase(widget);
    update();
}

void
Panner::viewManagerCentreFrameChanged(void *p, unsigned long f, bool)
{
//    std::cerr << "Panner[" << this << "]::viewManagerCentreFrameChanged(" 
//	      << p << ", " << f << ")" << std::endl;

    if (p == this) return;
    if (m_widgets.find(p) != m_widgets.end()) {
	m_widgets[p].first = f;
	update();
    }
}

void
Panner::viewManagerZoomLevelChanged(void *p, unsigned long z, bool)
{
    if (p == this) return;
    if (m_widgets.find(p) != m_widgets.end()) {
	m_widgets[p].second = z;
	update();
    }
}

void
Panner::viewManagerPlaybackFrameChanged(unsigned long f)
{
    bool changed = false;

    if (m_playPointerFrame / m_zoomLevel != f / m_zoomLevel) changed = true;
    m_playPointerFrame = f;

    for (WidgetMap::iterator i = m_widgets.begin(); i != m_widgets.end(); ++i) {
	unsigned long of = i->second.first;
	i->second.first = f;
	if (of / m_zoomLevel != f / m_zoomLevel) changed = true;
    }

    if (changed) update();
}

void
Panner::paintEvent(QPaintEvent *e)
{
/*!!!
    // Force View to recalculate zoom in case the size of the
    // widget has changed.  (We need a better name/mechanism for this)
    m_newModel = true;
*/

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
    long prevCentre = 0;
    long prevZoom = -1;

    for (WidgetMap::iterator i = m_widgets.begin(); i != m_widgets.end(); ++i) {
	if (!i->first) continue;

	View *w = (View *)i->first;
	if (i->second.second < 0) i->second.second = w->getZoomLevel();
	if (i->second.second < 0) continue;

	long c = (long)i->second.first;
	long z = (long)i->second.second;

	long f0 = c - (w->width() / 2) * z;
	long f1 = c + (w->width() / 2) * z;

	int x0 = (f0 - long(getCentreFrame())) / getZoomLevel() + width()/2;
	int x1 = (f1 - long(getCentreFrame())) / getZoomLevel() + width()/2 - 1;

	if (c != prevCentre || z != prevZoom) {
	    y += height() / 10 + 1;
	    prevCentre = c;
	    prevZoom = z;
	}
	
	paint.drawRect(x0, y, x1 - x0, height() - 2 * y);
    }

    paint.end();
}

void
Panner::mousePressEvent(QMouseEvent *e)
{
    m_clickPos = e->pos();
    for (WidgetMap::iterator i = m_widgets.begin(); i != m_widgets.end(); ++i) {
	if (i->first && i->second.second >= 0) {
	    m_clickedInRange = true;
	    m_dragCentreFrame = i->second.first;
	}
    }
}

void
Panner::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_clickedInRange) {
	mouseMoveEvent(e);
    }
    m_clickedInRange = false;
}

void
Panner::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_clickedInRange) return;

/*!!!
    long newFrame = getStartFrame() + e->x() * m_zoomLevel;

    if (newFrame < 0) newFrame = 0;
    if (newFrame >= getModelsEndFrame()) {
	newFrame = getModelsEndFrame();
	if (newFrame > 0) --newFrame;
    }
    emit centreFrameChanged(this, newFrame, true);
*/

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

#ifdef INCLUDE_MOCFILES
#include "Panner.moc.cpp"
#endif

