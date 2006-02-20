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
    m_navigating(false),
    m_resizing(false),
    m_centreLineVisible(true)
{
    setObjectName("Pane");
    setMouseTracking(true);
}

bool
Pane::shouldIlluminateLocalFeatures(const Layer *layer, QPoint &pos)
{
    if (layer == getSelectedLayer()) {
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

	    QPoint pos = m_identifyPoint;
	    QString desc = (*vi)->getFeatureDescription(pos);
	    
	    if (desc != "") {

		paint.save();

		int tabStop =
		    paint.fontMetrics().width(tr("Some lengthy prefix:"));

		QRect boundingRect = 
		    paint.fontMetrics().boundingRect
		    (rect(),
		     Qt::AlignRight | Qt::AlignTop | Qt::TextExpandTabs,
		     desc, tabStop);

		if (hasLightBackground()) {
		    paint.setPen(Qt::NoPen);
		    paint.setBrush(QColor(250, 250, 250, 200));
		} else {
		    paint.setPen(Qt::NoPen);
		    paint.setBrush(QColor(50, 50, 50, 200));
		}

		int extra = paint.fontMetrics().descent();
		paint.drawRect(width() - boundingRect.width() - 10 - extra,
			       10 - extra,
			       boundingRect.width() + 2 * extra,
			       boundingRect.height() + extra);

		if (hasLightBackground()) {
		    paint.setPen(QColor(150, 20, 0));
		} else {
		    paint.setPen(QColor(255, 150, 100));
		}
		
		QTextOption option;
		option.setWrapMode(QTextOption::NoWrap);
		option.setAlignment(Qt::AlignRight | Qt::AlignTop);
		option.setTabStop(tabStop);
		paint.drawText(QRectF(width() - boundingRect.width() - 10, 10,
				      boundingRect.width(),
				      boundingRect.height()),
			       desc,
			       option);

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
	if (m_manager && (m_manager->getToolMode() == ViewManager::NavigateMode)) {
	    //!!! be nice if this looked a bit more in keeping with the
	    //selection block
	    paint.setPen(Qt::blue);
	    paint.drawRect(m_clickPos.x(), m_clickPos.y(),
			   m_mousePos.x() - m_clickPos.x(),
			   m_mousePos.y() - m_clickPos.y());
	}
    }
    
    paint.end();
}

Selection
Pane::getSelectionAt(int x, bool &closeToLeftEdge, bool &closeToRightEdge)
{
    closeToLeftEdge = closeToRightEdge = false;

    if (!m_manager) return Selection();

    long testFrame = getFrameForX(x - 5);
    if (testFrame < 0) {
	testFrame = getFrameForX(x);
	if (testFrame < 0) return Selection();
    }

    Selection selection = m_manager->getContainingSelection(testFrame, true);
    if (selection.isEmpty()) return selection;

    int lx = getXForFrame(selection.getStartFrame());
    int rx = getXForFrame(selection.getEndFrame());
    
    int fuzz = 2;
    if (x < lx - fuzz || x > rx + fuzz) return Selection();

    int width = rx - lx;
    fuzz = 3;
    if (width < 12) fuzz = width / 4;
    if (fuzz < 1) fuzz = 1;

    if (x < lx + fuzz) closeToLeftEdge = true;
    if (x > rx - fuzz) closeToRightEdge = true;

    return selection;
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

    m_navigating = false;

    if (mode == ViewManager::NavigateMode || (e->buttons() & Qt::MidButton)) {

	if (mode != ViewManager::NavigateMode) {
	    setCursor(Qt::PointingHandCursor);
	}

	m_navigating = true;
	m_dragCentreFrame = m_centreFrame;

    } else if (mode == ViewManager::SelectMode) {

	bool closeToLeft = false, closeToRight = false;
	Selection selection = getSelectionAt(e->x(), closeToLeft, closeToRight);

	if ((closeToLeft || closeToRight) && !(closeToLeft && closeToRight)) {

	    m_manager->removeSelection(selection);

	    if (closeToLeft) {
		m_selectionStartFrame = selection.getEndFrame();
	    } else {
		m_selectionStartFrame = selection.getStartFrame();
	    }

	    m_manager->setInProgressSelection(selection, false);
	    m_resizing = true;
	
	} else {

	    int mouseFrame = getFrameForX(e->x());
	    size_t resolution = 1;
	    int snapFrame = mouseFrame;
	
	    Layer *layer = getSelectedLayer();
	    if (layer) {
		layer->snapToFeatureFrame(snapFrame, resolution, Layer::SnapLeft);
	    }
	    
	    if (snapFrame < 0) snapFrame = 0;
	    m_selectionStartFrame = snapFrame;
	    if (m_manager) {
		m_manager->setInProgressSelection(Selection(snapFrame,
							    snapFrame + resolution),
						  !m_ctrlPressed);
	    }

	    m_resizing = false;
	}

	update();

    } else if (mode == ViewManager::DrawMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->drawStart(e);
	}

    } else if (mode == ViewManager::EditMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->editStart(e);
	}
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

    if (m_navigating || mode == ViewManager::NavigateMode) {

	m_navigating = false;

	if (mode != ViewManager::NavigateMode) {
	    // restore cursor
	    toolModeChanged();
	}

	if (m_shiftPressed) {

	    int x0 = std::min(m_clickPos.x(), m_mousePos.x());
	    int x1 = std::max(m_clickPos.x(), m_mousePos.x());
	    int w = x1 - x0;
	    
	    long newStartFrame = getFrameForX(x0);
	    
	    long visibleFrames = getEndFrame() - getStartFrame();
	    if (newStartFrame <= -visibleFrames) {
		newStartFrame  = -visibleFrames + 1;
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

    } else if (mode == ViewManager::DrawMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->drawEnd(e);
	    update();
	}

    } else if (mode == ViewManager::EditMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->editEnd(e);
	    update();
	}
    }

    m_clickedInRange = false;

    emit paneInteractedWith();
}

void
Pane::mouseMoveEvent(QMouseEvent *e)
{
    ViewManager::ToolMode mode = ViewManager::NavigateMode;
    if (m_manager) mode = m_manager->getToolMode();

    QPoint prevPoint = m_identifyPoint;
    m_identifyPoint = e->pos();

    if (!m_clickedInRange) {
	
	if (mode == ViewManager::SelectMode) {
	    bool closeToLeft = false, closeToRight = false;
	    getSelectionAt(e->x(), closeToLeft, closeToRight);
	    if ((closeToLeft || closeToRight) && !(closeToLeft && closeToRight)) {
		setCursor(Qt::SizeHorCursor);
	    } else {
		setCursor(Qt::ArrowCursor);
	    }
	}

//!!!	if (mode != ViewManager::DrawMode) {

	    bool previouslyIdentifying = m_identifyFeatures;
	    m_identifyFeatures = true;
	    
	    if (m_identifyFeatures != previouslyIdentifying ||
		m_identifyPoint != prevPoint) {
		update();
	    }
//	}

	return;
    }

    if (m_navigating || mode == ViewManager::NavigateMode) {

	if (m_shiftPressed) {

	    m_mousePos = e->pos();
	    update();

	} else {

	    long frameOff = getFrameForX(e->x()) - getFrameForX(m_clickPos.x());

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

	    if (getXForFrame(m_centreFrame) != getXForFrame(newCentreFrame)) {
		setCentreFrame(newCentreFrame);
	    }
	}

    } else if (mode == ViewManager::SelectMode) {

	int mouseFrame = getFrameForX(e->x());
	size_t resolution = 1;
	int snapFrameLeft = mouseFrame;
	int snapFrameRight = mouseFrame;
	
	Layer *layer = getSelectedLayer();
	if (layer) {
	    layer->snapToFeatureFrame(snapFrameLeft, resolution, Layer::SnapLeft);
	    layer->snapToFeatureFrame(snapFrameRight, resolution, Layer::SnapRight);
	}
	
	std::cerr << "snap: frame = " << mouseFrame << ", start frame = " << m_selectionStartFrame << ", left = " << snapFrameLeft << ", right = " << snapFrameRight << std::endl;

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
					      !m_resizing && !m_ctrlPressed);
	}

	bool doScroll = false;
	if (!m_manager) doScroll = true;
	if (!m_manager->isPlaying()) doScroll = true;
	if (m_followPlay != PlaybackScrollContinuous) doScroll = true;

	if (doScroll) {
	    int offset = mouseFrame - getStartFrame();
	    int available = getEndFrame() - getStartFrame();
	    if (offset >= available * 0.95) {
		int move = int(offset - available * 0.95) + 1;
		setCentreFrame(m_centreFrame + move);
	    } else if (offset <= available * 0.10) {
		int move = int(available * 0.10 - offset) + 1;
		if (m_centreFrame > move) {
		    setCentreFrame(m_centreFrame - move);
		} else {
		    setCentreFrame(0);
		}
	    }
	}

	update();

    } else if (mode == ViewManager::DrawMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->drawDrag(e);
	}

    } else if (mode == ViewManager::EditMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->editDrag(e);
	}
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

    int count = e->delta();

    if (count > 0) {
	if (count >= 120) count /= 120;
	else count = 1;
    } 

    if (count < 0) {
	if (count <= -120) count /= 120;
	else count = -1;
    }

    if (e->modifiers() & Qt::ControlModifier) {

	// Scroll left or right, rapidly

	if (getStartFrame() < 0 && 
	    getEndFrame() >= getModelsEndFrame()) return;

	long delta = ((width() / 2) * count * m_zoomLevel);

	if (int(m_centreFrame) < delta) {
	    setCentreFrame(0);
	} else if (int(m_centreFrame) - delta >= int(getModelsEndFrame())) {
	    setCentreFrame(getModelsEndFrame());
	} else {
	    setCentreFrame(m_centreFrame - delta);
	}

    } else {

	// Zoom in or out

	int newZoomLevel = m_zoomLevel;
  
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
	setCursor(Qt::UpArrowCursor);
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

