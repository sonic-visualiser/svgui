/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "widgets/Pane.h"
#include "base/Layer.h"
#include "base/Model.h"
#include "base/ZoomConstraint.h"
#include "base/RealTime.h"
#include "base/Profiler.h"
#include "base/ViewManager.h"
#include "base/CommandHistory.h"
#include "layer/WaveformLayer.h"

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
Pane::shouldIlluminateLocalFeatures(const Layer *layer, QPoint &pos) const
{
    QPoint discard;
    bool b0, b1;

    if (layer == getSelectedLayer() &&
	!shouldIlluminateLocalSelection(discard, b0, b1)) {

	pos = m_identifyPoint;
	return m_identifyFeatures;
    }

    return false;
}

bool
Pane::shouldIlluminateLocalSelection(QPoint &pos,
				     bool &closeToLeft,
				     bool &closeToRight) const
{
    if (m_identifyFeatures &&
	m_manager &&
	m_manager->getToolMode() == ViewManager::EditMode &&
	!m_manager->getSelections().empty() &&
	!selectionIsBeingEdited()) {

	Selection s(getSelectionAt(m_identifyPoint.x(),
				   closeToLeft, closeToRight));

	if (!s.isEmpty()) {
	    if (getSelectedLayer() && getSelectedLayer()->isLayerEditable()) {
		
		pos = m_identifyPoint;
		return true;
	    }
	}
    }

    return false;
}

bool
Pane::selectionIsBeingEdited() const
{
    if (!m_editingSelection.isEmpty()) {
	if (m_mousePos != m_clickPos &&
	    getFrameForX(m_mousePos.x()) != getFrameForX(m_clickPos.x())) {
	    return true;
	}
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

    const Model *waveformModel = 0; // just for reporting purposes
    int verticalScaleWidth = 0;
    
    int fontHeight = paint.fontMetrics().height();
    int fontAscent = paint.fontMetrics().ascent();

    if (m_manager &&
        !m_manager->isPlaying() &&
        m_manager->getToolMode() == ViewManager::SelectMode) {

        for (LayerList::iterator vi = m_layers.end(); vi != m_layers.begin(); ) {
            --vi;

            std::vector<QRect> crosshairExtents;

            if ((*vi)->getCrosshairExtents(this, paint, m_identifyPoint,
                                           crosshairExtents)) {
                (*vi)->paintCrosshairs(this, paint, m_identifyPoint);
                break;
            } else if ((*vi)->isLayerOpaque()) {
                break;
            }
        }
    }

    for (LayerList::iterator vi = m_layers.end(); vi != m_layers.begin(); ) {
        --vi;
            
        if (dynamic_cast<WaveformLayer *>(*vi)) {
            waveformModel = (*vi)->getModel();
        }

        if (!m_manager ||
            m_manager->getOverlayMode() == ViewManager::NoOverlays) {
            break;
        }

        verticalScaleWidth = (*vi)->getVerticalScaleWidth(this, paint);

        if (verticalScaleWidth > 0 && r.left() < verticalScaleWidth) {

//	    Profiler profiler("Pane::paintEvent - painting vertical scale", true);

//	    std::cerr << "Pane::paintEvent: calling paint.save() in vertical scale block" << std::endl;
            paint.save();
            
            paint.setPen(Qt::black);
            paint.setBrush(Qt::white);
            paint.drawRect(0, -1, verticalScaleWidth, height()+1);
            
            paint.setBrush(Qt::NoBrush);
            (*vi)->paintVerticalScale
                (this, paint, QRect(0, 0, verticalScaleWidth, height()));
            
            paint.restore();
        }
	
        if (m_identifyFeatures) {
            
            QPoint pos = m_identifyPoint;
            QString desc = (*vi)->getFeatureDescription(this, pos);
	    
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
    
    int sampleRate = getModelsSampleRate();
    paint.setBrush(Qt::NoBrush);

    if (m_centreLineVisible) {

	if (hasLightBackground()) {
	    paint.setPen(QColor(50, 50, 50));
	} else {
	    paint.setPen(QColor(200, 200, 200));
	}	
	paint.drawLine(width() / 2, 0, width() / 2, height() - 1);

	paint.setPen(QColor(50, 50, 50));

	int y = height() - fontHeight
	    + fontAscent - 6;
	
	LayerList::iterator vi = m_layers.end();
	
	if (vi != m_layers.begin()) {
	    
	    switch ((*--vi)->getPreferredFrameCountPosition()) {
		
	    case Layer::PositionTop:
		y = fontAscent + 6;
		break;
		
	    case Layer::PositionMiddle:
		y = (height() - fontHeight) / 2
		    + fontAscent;
		break;

	    case Layer::PositionBottom:
		// y already set correctly
		break;
	    }
	}

        if (m_manager &&
            m_manager->getOverlayMode() != ViewManager::NoOverlays) {

            if (sampleRate) {

                QString text(QString::fromStdString
                             (RealTime::frame2RealTime
                              (m_centreFrame, sampleRate).toText(true)));
                
                int tw = paint.fontMetrics().width(text);
                int x = width()/2 - 4 - tw;
                
                drawVisibleText(paint, x, y, text, OutlinedText);
            }
            
            QString text = QString("%1").arg(m_centreFrame);
            
            int tw = paint.fontMetrics().width(text);
            int x = width()/2 + 4;
            
            drawVisibleText(paint, x, y, text, OutlinedText);
        }

    } else {

	paint.setPen(QColor(50, 50, 50));
    }

    if (waveformModel &&
        m_manager &&
        m_manager->getOverlayMode() != ViewManager::NoOverlays &&
	r.y() + r.height() >= height() - fontHeight - 6) {

	size_t mainModelRate = m_manager->getMainModelSampleRate();
	size_t playbackRate = m_manager->getPlaybackSampleRate();
	    
	QString srNote = "";

	// Show (R) for waveform models that will be resampled on
	// playback, and (X) for waveform models that will be played
	// at the wrong rate because their rate differs from that of
	// the main model.

	if (sampleRate == mainModelRate) {
	    if (sampleRate != playbackRate) srNote = " " + tr("(R)");
	} else {
	    std::cerr << "Sample rate = " << sampleRate << ", main model rate = " << mainModelRate << std::endl;
	    srNote = " " + tr("(X)");
	}

	QString desc = tr("%1 / %2Hz%3")
	    .arg(RealTime::frame2RealTime(waveformModel->getEndFrame(),
					  sampleRate)
		 .toText(false).c_str())
	    .arg(sampleRate)
	    .arg(srNote);

	if (r.x() < verticalScaleWidth + 5 + paint.fontMetrics().width(desc)) {
	    drawVisibleText(paint, verticalScaleWidth + 5,
			    height() - fontHeight + fontAscent - 6,
			    desc, OutlinedText);
	}
    }

    if (m_manager &&
        m_manager->getOverlayMode() == ViewManager::AllOverlays &&
        r.y() + r.height() >= height() - m_layers.size() * fontHeight - 6) {

	std::vector<QString> texts;
	int maxTextWidth = 0;

	for (LayerList::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {

	    QString text = (*i)->getLayerPresentationName();
	    int tw = paint.fontMetrics().width(text);
            bool reduced = false;
            while (tw > width() / 3 && text.length() > 4) {
                if (!reduced && text.length() > 8) {
                    text = text.left(text.length() - 4);
                } else {
                    text = text.left(text.length() - 2);
                }
                reduced = true;
                tw = paint.fontMetrics().width(text + "...");
            }
            if (reduced) {
                texts.push_back(text + "...");
            } else {
                texts.push_back(text);
            }
	    if (tw > maxTextWidth) maxTextWidth = tw;
	}
    
	int lly = height() - 6;

	if (r.x() + r.width() >= width() - maxTextWidth - 5) {
	    
	    for (int i = 0; i < texts.size(); ++i) {

		if (i == texts.size() - 1) {
		    paint.setPen(Qt::black);
		}
		
		drawVisibleText(paint, width() - maxTextWidth - 5,
				lly - fontHeight + fontAscent,
				texts[i], OutlinedText);
		
		lly -= fontHeight;
	    }
	}
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
    
    if (selectionIsBeingEdited()) {

	int offset = m_mousePos.x() - m_clickPos.x();
	int p0 = getXForFrame(m_editingSelection.getStartFrame()) + offset;
	int p1 = getXForFrame(m_editingSelection.getEndFrame()) + offset;

	if (m_editingSelectionEdge < 0) {
	    p1 = getXForFrame(m_editingSelection.getEndFrame());
	} else if (m_editingSelectionEdge > 0) {
	    p0 = getXForFrame(m_editingSelection.getStartFrame());
	}

	paint.save();
	if (hasLightBackground()) {
	    paint.setPen(QPen(Qt::black, 2));
	} else {
	    paint.setPen(QPen(Qt::white, 2));
	}

	//!!! duplicating display policy with View::drawSelections

	if (m_editingSelectionEdge < 0) {
	    paint.drawLine(p0, 1, p1, 1);
	    paint.drawLine(p0, 0, p0, height());
	    paint.drawLine(p0, height() - 1, p1, height() - 1);
	} else if (m_editingSelectionEdge > 0) {
	    paint.drawLine(p0, 1, p1, 1);
	    paint.drawLine(p1, 0, p1, height());
	    paint.drawLine(p0, height() - 1, p1, height() - 1);
	} else {
	    paint.setBrush(Qt::NoBrush);
	    paint.drawRect(p0, 1, p1 - p0, height() - 2);
	}
	paint.restore();
    }

    paint.end();
}

Selection
Pane::getSelectionAt(int x, bool &closeToLeftEdge, bool &closeToRightEdge) const
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
    if (e->buttons() & Qt::RightButton) {
        emit rightButtonMenuRequested(mapToGlobal(e->pos()));
        return;
    }

    m_clickPos = e->pos();
    m_clickedInRange = true;
    m_editingSelection = Selection();
    m_editingSelectionEdge = 0;
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
	    if (layer && !m_shiftPressed) {
		layer->snapToFeatureFrame(this, snapFrame,
					  resolution, Layer::SnapLeft);
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
	    layer->drawStart(this, e);
	}

    } else if (mode == ViewManager::EditMode) {

	if (!editSelectionStart(e)) {
	    Layer *layer = getSelectedLayer();
	    if (layer && layer->isLayerEditable()) {
		layer->editStart(this, e);
	    }
	}
    }

    emit paneInteractedWith();
}

void
Pane::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::RightButton) {
        return;
    }

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

	    int y0 = std::min(m_clickPos.y(), m_mousePos.y());
	    int y1 = std::max(m_clickPos.y(), m_mousePos.y());
//	    int h = y1 - y0;
	    
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

            //!!! lots of faff, shouldn't be here

            QString unit;
            float min, max;
            bool log;
            Layer *layer = 0;
            for (LayerList::const_iterator i = m_layers.begin();
                 i != m_layers.end(); ++i) { 
                if ((*i)->getValueExtents(min, max, log, unit) &&
                    (*i)->getDisplayExtents(min, max)) {
                    layer = *i;
                    break;
                }
            }
            
            if (layer) {
                if (log) {
                    min = (min < 0.0) ? -log10f(-min) : (min == 0.0) ? 0.0 : log10f(min);
                    max = (max < 0.0) ? -log10f(-max) : (max == 0.0) ? 0.0 : log10f(max);
                }
                float rmin = min + ((max - min) * (height() - y1)) / height();
                float rmax = min + ((max - min) * (height() - y0)) / height();
                std::cerr << "min: " << min << ", max: " << max << ", y0: " << y0 << ", y1: " << y1 << ", h: " << height() << ", rmin: " << rmin << ", rmax: " << rmax << std::endl;
                if (log) {
                    rmin = powf(10, rmin);
                    rmax = powf(10, rmax);
                }
                std::cerr << "finally: rmin: " << rmin << ", rmax: " << rmax << " " << unit.toStdString() << std::endl;

                layer->setDisplayExtents(rmin, rmax);
            }
                
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
	    layer->drawEnd(this, e);
	    update();
	}

    } else if (mode == ViewManager::EditMode) {

	if (!editSelectionEnd(e)) {
	    Layer *layer = getSelectedLayer();
	    if (layer && layer->isLayerEditable()) {
		layer->editEnd(this, e);
		update();
	    }
	}
    }

    m_clickedInRange = false;

    emit paneInteractedWith();
}

void
Pane::mouseMoveEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::RightButton) {
        return;
    }

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

        if (!m_manager->isPlaying()) {

	if (getSelectedLayer()) {

	    bool previouslyIdentifying = m_identifyFeatures;
	    m_identifyFeatures = true;
	    
	    if (m_identifyFeatures != previouslyIdentifying ||
		m_identifyPoint != prevPoint) {
		update();
	    }
	}

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
	if (layer && !m_shiftPressed) {
	    layer->snapToFeatureFrame(this, snapFrameLeft,
				      resolution, Layer::SnapLeft);
	    layer->snapToFeatureFrame(this, snapFrameRight,
				      resolution, Layer::SnapRight);
	}
	
//	std::cerr << "snap: frame = " << mouseFrame << ", start frame = " << m_selectionStartFrame << ", left = " << snapFrameLeft << ", right = " << snapFrameRight << std::endl;

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
	    layer->drawDrag(this, e);
	}

    } else if (mode == ViewManager::EditMode) {

	if (!editSelectionDrag(e)) {
	    Layer *layer = getSelectedLayer();
	    if (layer && layer->isLayerEditable()) {
		layer->editDrag(this, e);
	    }
	}
    }
}

void
Pane::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->buttons() & Qt::RightButton) {
        return;
    }

//    std::cerr << "mouseDoubleClickEvent" << std::endl;

    m_clickPos = e->pos();
    m_clickedInRange = true;
    m_shiftPressed = (e->modifiers() & Qt::ShiftModifier);
    m_ctrlPressed = (e->modifiers() & Qt::ControlModifier);

    ViewManager::ToolMode mode = ViewManager::NavigateMode;
    if (m_manager) mode = m_manager->getToolMode();

    if (mode == ViewManager::NavigateMode ||
        mode == ViewManager::EditMode) {

	Layer *layer = getSelectedLayer();
	if (layer && layer->isLayerEditable()) {
	    layer->editOpen(this, e);
	}
    }
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

bool
Pane::editSelectionStart(QMouseEvent *e)
{
    if (!m_identifyFeatures ||
	!m_manager ||
	m_manager->getToolMode() != ViewManager::EditMode) {
	return false;
    }

    bool closeToLeft, closeToRight;
    Selection s(getSelectionAt(e->x(), closeToLeft, closeToRight));
    if (s.isEmpty()) return false;
    m_editingSelection = s;
    m_editingSelectionEdge = (closeToLeft ? -1 : closeToRight ? 1 : 0);
    m_mousePos = e->pos();
    return true;
}

bool
Pane::editSelectionDrag(QMouseEvent *e)
{
    if (m_editingSelection.isEmpty()) return false;
    m_mousePos = e->pos();
    update();
    return true;
}

bool
Pane::editSelectionEnd(QMouseEvent *e)
{
    if (m_editingSelection.isEmpty()) return false;

    int offset = m_mousePos.x() - m_clickPos.x();
    Layer *layer = getSelectedLayer();

    if (offset == 0 || !layer) {
	m_editingSelection = Selection();
	return true;
    }

    int p0 = getXForFrame(m_editingSelection.getStartFrame()) + offset;
    int p1 = getXForFrame(m_editingSelection.getEndFrame()) + offset;

    long f0 = getFrameForX(p0);
    long f1 = getFrameForX(p1);

    Selection newSelection(f0, f1);
    
    if (m_editingSelectionEdge == 0) {
	
        CommandHistory::getInstance()->startCompoundOperation
            (tr("Drag Selection"), true);

	layer->moveSelection(m_editingSelection, f0);
	
    } else {
	
        CommandHistory::getInstance()->startCompoundOperation
            (tr("Resize Selection"), true);

	if (m_editingSelectionEdge < 0) {
	    f1 = m_editingSelection.getEndFrame();
	} else {
	    f0 = m_editingSelection.getStartFrame();
	}

	newSelection = Selection(f0, f1);
	layer->resizeSelection(m_editingSelection, newSelection);
    }
    
    m_manager->removeSelection(m_editingSelection);
    m_manager->addSelection(newSelection);

    CommandHistory::getInstance()->endCompoundOperation();

    m_editingSelection = Selection();
    return true;
}

void
Pane::toolModeChanged()
{
    ViewManager::ToolMode mode = m_manager->getToolMode();
//    std::cerr << "Pane::toolModeChanged(" << mode << ")" << std::endl;

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
/*	
    case ViewManager::TextMode:
	setCursor(Qt::IBeamCursor);
	break;
*/
    }
}

QString
Pane::toXmlString(QString indent, QString extraAttributes) const
{
    return View::toXmlString
	(indent,
	 QString("type=\"pane\" centreLineVisible=\"%1\" height=\"%2\" %3")
	 .arg(m_centreLineVisible).arg(height()).arg(extraAttributes));
}


#ifdef INCLUDE_MOCFILES
#include "Pane.moc.cpp"
#endif

