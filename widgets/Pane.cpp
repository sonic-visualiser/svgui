/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "widgets/Pane.h"
#include "base/Layer.h"
#include "base/Model.h"
#include "base/ZoomConstraint.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/ViewManager.h"

#include <QPaintEvent>
#include <QPainter>
#include <iostream>
#include <cmath>

using std::cerr;
using std::endl;

Pane::Pane(QWidget *w) :
    View(w, true),
    m_identifyFeatures(false),
    m_clickedInRange(false),
    m_shiftPressed(false),
    m_ctrlPressed(false),
    m_centreLineVisible(true)
{
    setObjectName("Pane");
    setMouseTracking(true);
}

bool
Pane::shouldIlluminateLocalFeatures(const Layer *layer, QPoint &pos)
{
    for (LayerList::iterator vi = m_layers.end(); vi != m_layers.begin(); ) {
	--vi;
	if (layer != *vi) return false;
	pos = m_identifyPoint;
	return m_identifyFeatures;
    }

    return false;
}

void
Pane::setCentreLineVisible(bool visible)
{
    m_centreLineVisible = visible;
    update();
}

void
Pane::paintEvent(QPaintEvent *e)
{
    QPainter paint;

    QRect r(rect());

    if (e) {
	r = e->rect();
    }
/*
    paint.begin(this);
    paint.setClipRect(r);

    if (hasLightBackground()) {
	paint.setPen(Qt::white);
	paint.setBrush(Qt::white);
    } else {
	paint.setPen(Qt::black);
	paint.setBrush(Qt::black);
    }
    paint.drawRect(r);

    paint.end();
*/
    View::paintEvent(e);

    paint.begin(this);

    if (e) {
	paint.setClipRect(r);
    }
	
    for (LayerList::iterator vi = m_layers.end(); vi != m_layers.begin(); ) {
	--vi;

	int sw = (*vi)->getVerticalScaleWidth(paint);

	if (sw > 0 && r.left() < sw) {

//	    Profiler profiler("Pane::paintEvent - painting vertical scale", true);

//	    std::cerr << "Pane::paintEvent: calling paint.save() in vertical scale block" << std::endl;
	    paint.save();

	    paint.setPen(Qt::black);
	    paint.setBrush(Qt::white);
	    paint.drawRect(0, 0, sw, height());

	    paint.setBrush(Qt::NoBrush);
	    (*vi)->paintVerticalScale(paint, QRect(0, 0, sw, height()));

	    paint.restore();
	}

	if (m_identifyFeatures) {
	    QRect descRect = (*vi)->getFeatureDescriptionRect(paint,
							      m_identifyPoint);
	    if (descRect.width() > 0 && descRect.height() > 0 &&
		r.left() + r.width() >= width() - descRect.width() &&
		r.top() < descRect.height()) {
		
//		Profiler profiler("Pane::paintEvent - painting local feature description", true);
		
//		std::cerr << "Pane::paintEvent: calling paint.save() in feature description block" << std::endl;
		paint.save();
		
		paint.setPen(Qt::black);
		paint.setBrush(Qt::white);
		
		QRect rect(width() - descRect.width() - 1, 0,
			   descRect.width(), descRect.height());
		
		paint.drawRect(rect);
		
		paint.setBrush(Qt::NoBrush);
		(*vi)->paintLocalFeatureDescription(paint, rect, m_identifyPoint);
		
		paint.restore();
	    }
	}

	break;
    }
    
    if (m_centreLineVisible) {

	if (hasLightBackground()) {
	    paint.setPen(QColor(50, 50, 50));
	} else {
	    paint.setPen(QColor(200, 200, 200));
	}	
	paint.setBrush(Qt::NoBrush);
	paint.drawLine(width() / 2, 0, width() / 2, height() - 1);
	
//    QFont font(paint.font());
//    font.setBold(true);
//    paint.setFont(font);

	int sampleRate = getModelsSampleRate();
	int y = height() - paint.fontMetrics().height()
	    + paint.fontMetrics().ascent() - 6;
	
	LayerList::iterator vi = m_layers.end();
	
	if (vi != m_layers.begin()) {
	    
	    switch ((*--vi)->getPreferredFrameCountPosition()) {
		
	    case Layer::PositionTop:
		y = paint.fontMetrics().ascent() + 6;
		break;
		
	    case Layer::PositionMiddle:
		y = (height() - paint.fontMetrics().height()) / 2
		    + paint.fontMetrics().ascent();
		break;

	    case Layer::PositionBottom:
		// y already set correctly
		break;
	    }
	}

	if (sampleRate) {

	    QString text(QString::fromStdString
			 (RealTime::frame2RealTime
			  (m_centreFrame, sampleRate).toText(true)));

	    int tw = paint.fontMetrics().width(text);
	    int x = width()/2 - 4 - tw;

	    if (hasLightBackground()) {
		paint.setPen(palette().background().color());
		for (int dx = -1; dx <= 1; ++dx) {
		    for (int dy = -1; dy <= 1; ++dy) {
			if ((dx && dy) || !(dx || dy)) continue;
			paint.drawText(x + dx, y + dy, text);
		    }
		}
		paint.setPen(QColor(50, 50, 50));
	    } else {
		paint.setPen(QColor(200, 200, 200));
	    }

	    paint.drawText(x, y, text);
	}

	QString text = QString("%1").arg(m_centreFrame);

	int tw = paint.fontMetrics().width(text);
	int x = width()/2 + 4;
    
	if (hasLightBackground()) {
	    paint.setPen(palette().background().color());
	    for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
		    if ((dx && dy) || !(dx || dy)) continue;
		    paint.drawText(x + dx, y + dy, text);
		}
	    }
	    paint.setPen(QColor(50, 50, 50));
	} else {
	    paint.setPen(QColor(200, 200, 200));
	}
	paint.drawText(x, y, text);
    }

    if (m_clickedInRange && m_shiftPressed) {
	//!!! be nice if this looked a bit more in keeping with the
	//selection block
	paint.setPen(Qt::blue);
	paint.drawRect(m_clickPos.x(), m_clickPos.y(),
		       m_mousePos.x() - m_clickPos.x(),
		       m_mousePos.y() - m_clickPos.y());
    }
    
    paint.end();
}

void
Pane::mousePressEvent(QMouseEvent *e)
{
    m_clickPos = e->pos();
    m_clickedInRange = true;
    m_shiftPressed = (e->modifiers() & Qt::ShiftModifier);
    m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

    ViewManager::ToolMode mode = ViewManager::NavigateMode;
    if (m_manager) mode = m_manager->getToolMode();

    if (mode == ViewManager::NavigateMode) {

	m_dragCentreFrame = m_centreFrame;

    } else if (mode == ViewManager::SelectMode) {

	int mouseFrame = e->x() * m_zoomLevel + getStartFrame();
	size_t resolution = 1;
	int snapFrame = mouseFrame;
	
	Layer *layer = getSelectedLayer();
	if (layer) {
	    snapFrame = layer->getNearestFeatureFrame(mouseFrame, resolution,
						      false);
	}
	
	if (snapFrame < 0) snapFrame = 0;
	m_selectionStartFrame = snapFrame;
	if (m_manager) {
	    m_manager->setInProgressSelection(Selection(snapFrame,
							snapFrame + resolution),
					      !m_ctrlPressed);
	}
	update();
    }

    emit paneInteractedWith();
}

void
Pane::mouseReleaseEvent(QMouseEvent *e)
{
    ViewManager::ToolMode mode = ViewManager::NavigateMode;
    if (m_manager) mode = m_manager->getToolMode();

    if (m_clickedInRange) {
	mouseMoveEvent(e);
    }

    if (mode == ViewManager::NavigateMode) {

	if (m_shiftPressed) {

	    int x0 = std::min(m_clickPos.x(), m_mousePos.x());
	    int x1 = std::max(m_clickPos.x(), m_mousePos.x());
	    int w = x1 - x0;
	    
	    long newStartFrame = getStartFrame() + m_zoomLevel * x0;
	    
	    if (newStartFrame <= -long(width() * m_zoomLevel)) {
		newStartFrame  = -long(width() * m_zoomLevel) + 1;
	    }
	    
	    if (newStartFrame >= long(getModelsEndFrame())) {
		newStartFrame  = getModelsEndFrame() - 1;
	    }
	    
	    float ratio = float(w) / float(width());
//	std::cerr << "ratio: " << ratio << std::endl;
	    size_t newZoomLevel = (size_t)nearbyint(m_zoomLevel * ratio);
	    if (newZoomLevel < 1) newZoomLevel = 1;

//	std::cerr << "start: " << m_startFrame << ", level " << m_zoomLevel << std::endl;
	    setZoomLevel(getZoomConstraintBlockSize(newZoomLevel));
	    setStartFrame(newStartFrame);

	    //cerr << "mouseReleaseEvent: start frame now " << m_startFrame << endl;
//	update();
	}

    } else if (mode == ViewManager::SelectMode) {

	if (m_manager && m_manager->haveInProgressSelection()) {

	    bool exclusive;
	    Selection selection = m_manager->getInProgressSelection(exclusive);
	    
	    if (selection.getEndFrame() < selection.getStartFrame() + 2) {
		selection = Selection();
	    }
	    
	    m_manager->clearInProgressSelection();
	    
	    if (exclusive) {
		m_manager->setSelection(selection);
	    } else {
		m_manager->addSelection(selection);
	    }
	}
	
	update();
    } 

    m_clickedInRange = false;

    emit paneInteractedWith();
}

void
Pane::mouseMoveEvent(QMouseEvent *e)
{
    ViewManager::ToolMode mode = ViewManager::NavigateMode;
    if (m_manager) mode = m_manager->getToolMode();

    if (!m_clickedInRange) {
	
//	std::cerr << "Pane: calling identifyLocalFeatures" << std::endl;

//!!!	identifyLocalFeatures(true, e->x(), e->y());

	bool previouslyIdentifying = m_identifyFeatures;
	QPoint prevPoint = m_identifyPoint;

	m_identifyFeatures = true;
	m_identifyPoint = e->pos();

	if (m_identifyFeatures != previouslyIdentifying ||
	    m_identifyPoint != prevPoint) {
	    update();
	}

	return;
	
    }

    if (mode == ViewManager::NavigateMode) {

	if (m_shiftPressed) {

	    m_mousePos = e->pos();
	    update();

	} else {

	    long xoff = int(e->x()) - int(m_clickPos.x());
	    long frameOff = xoff * m_zoomLevel;
	    
	    size_t newCentreFrame = m_dragCentreFrame;
	    
	    if (frameOff < 0) {
		newCentreFrame -= frameOff;
	    } else if (newCentreFrame >= size_t(frameOff)) {
		newCentreFrame -= frameOff;
	    } else {
		newCentreFrame = 0;
	    }
	    
	    if (newCentreFrame >= getModelsEndFrame()) {
		newCentreFrame = getModelsEndFrame();
		if (newCentreFrame > 0) --newCentreFrame;
	    }
	    
	    if (std::max(m_centreFrame, newCentreFrame) -
		std::min(m_centreFrame, newCentreFrame) > size_t(m_zoomLevel)) {
		setCentreFrame(newCentreFrame);
	    }
	}

    } else if (mode == ViewManager::SelectMode) {

	int mouseFrame = e->x() * m_zoomLevel + getStartFrame();
	size_t resolution = 1;
	int snapFrameLeft = mouseFrame;
	int snapFrameRight = mouseFrame;
	
	Layer *layer = getSelectedLayer();
	if (layer) {
	    snapFrameLeft = layer->getNearestFeatureFrame(mouseFrame, resolution,
							  false);
	    snapFrameRight = layer->getNearestFeatureFrame(mouseFrame, resolution,
							   true);
	}
	
	if (snapFrameLeft < 0) snapFrameLeft = 0;
	if (snapFrameRight < 0) snapFrameRight = 0;
	
	size_t min, max;
	
	if (m_selectionStartFrame > snapFrameLeft) {
	    min = snapFrameLeft;
	    max = m_selectionStartFrame;
	} else if (snapFrameRight > m_selectionStartFrame) {
	    min = m_selectionStartFrame;
	    max = snapFrameRight;
	} else {
	    min = snapFrameLeft;
	    max = snapFrameRight;
	}

	if (m_manager) {
	    m_manager->setInProgressSelection(Selection(min, max),
					      !m_ctrlPressed);
	}
	
	if (!m_manager || !m_manager->isPlaying()) {
	    int offset = mouseFrame - getStartFrame();
	    int available = getEndFrame() - getStartFrame();
	    if (offset >= available * 0.9) {
		int move = int(offset - available * 0.9) + 1;
		setCentreFrame(m_centreFrame + move);
	    } else if (offset <= available * 0.15) {
		int move = int(available * 0.15 - offset) + 1;
		if (m_centreFrame > move) {
		    setCentreFrame(m_centreFrame - move);
		} else {
		    setCentreFrame(0);
		}
	    }
	}

	update();
    }
}

void
Pane::mouseDoubleClickEvent(QMouseEvent *e)
{
    std::cerr << "mouseDoubleClickEvent" << std::endl;
}

void
Pane::leaveEvent(QEvent *)
{
    bool previouslyIdentifying = m_identifyFeatures;
    m_identifyFeatures = false;
    if (previouslyIdentifying) update();
}

void
Pane::wheelEvent(QWheelEvent *e)
{
    //std::cerr << "wheelEvent, delta " << e->delta() << std::endl;

    int newZoomLevel = m_zoomLevel;

    int count = e->delta();

    if (count > 0) {
	if (count >= 120) count /= 120;
	else count = 1;
    } 

    if (count < 0) {
	if (count <= -120) count /= 120;
	else count = -1;
    }
  
    while (count > 0) {
	if (newZoomLevel <= 2) {
	    newZoomLevel = 1;
	    break;
	}
	newZoomLevel = getZoomConstraintBlockSize(newZoomLevel - 1, 
						  ZoomConstraint::RoundDown);
	--count;
    }

    while (count < 0) {
	newZoomLevel = getZoomConstraintBlockSize(newZoomLevel + 1,
						  ZoomConstraint::RoundUp);
	++count;
    }

    if (newZoomLevel != m_zoomLevel) {
	setZoomLevel(newZoomLevel);
    }

    emit paneInteractedWith();
}

void
Pane::toolModeChanged()
{
    ViewManager::ToolMode mode = m_manager->getToolMode();
    std::cerr << "Pane::toolModeChanged(" << mode << ")" << std::endl;

    switch (mode) {

    case ViewManager::NavigateMode:
	setCursor(Qt::PointingHandCursor);
	break;
	
    case ViewManager::SelectMode:
	setCursor(Qt::ArrowCursor);
	break;
	
    case ViewManager::EditMode:
	setCursor(Qt::SizeAllCursor);
	break;
	
    case ViewManager::DrawMode:
	setCursor(Qt::CrossCursor);
	break;
	
    case ViewManager::TextMode:
	setCursor(Qt::IBeamCursor);
	break;
    }
}

QString
Pane::toXmlString(QString indent, QString extraAttributes) const
{
    return View::toXmlString
	(indent,
	 QString("type=\"pane\" centreLineVisible=\"%1\" %2")
	 .arg(m_centreLineVisible).arg(extraAttributes));
}


#ifdef INCLUDE_MOCFILES
#include "Pane.moc.cpp"
#endif

