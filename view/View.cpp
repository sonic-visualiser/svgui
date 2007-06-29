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

#include "View.h"
#include "layer/Layer.h"
#include "data/model/Model.h"
#include "base/ZoomConstraint.h"
#include "base/Profiler.h"

#include "layer/TimeRulerLayer.h" //!!! damn, shouldn't be including that here
#include "data/model/PowerOfSqrtTwoZoomConstraint.h" //!!! likewise

#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QApplication>
#include <QProgressDialog>

#include <iostream>
#include <cassert>
#include <math.h>

//#define DEBUG_VIEW_WIDGET_PAINT 1

using std::cerr;
using std::endl;

View::View(QWidget *w, bool showProgress) :
    QFrame(w),
    m_centreFrame(0),
    m_zoomLevel(1024),
    m_followPan(true),
    m_followZoom(true),
    m_followPlay(PlaybackScrollPage),
    m_playPointerFrame(0),
    m_showProgress(showProgress),
    m_cache(0),
    m_cacheCentreFrame(0),
    m_cacheZoomLevel(1024),
    m_selectionCached(false),
    m_deleting(false),
    m_haveSelectedLayer(false),
    m_manager(0),
    m_propertyContainer(new ViewPropertyContainer(this))
{
}

View::~View()
{
//    std::cerr << "View::~View(" << this << ")" << std::endl;

    m_deleting = true;
    delete m_propertyContainer;
}

PropertyContainer::PropertyList
View::getProperties() const
{
    PropertyContainer::PropertyList list;
    list.push_back("Global Scroll");
    list.push_back("Global Zoom");
    list.push_back("Follow Playback");
    return list;
}

QString
View::getPropertyLabel(const PropertyName &pn) const
{
    if (pn == "Global Scroll") return tr("Global Scroll");
    if (pn == "Global Zoom") return tr("Global Zoom");
    if (pn == "Follow Playback") return tr("Follow Playback");
    return "";
}

PropertyContainer::PropertyType
View::getPropertyType(const PropertyContainer::PropertyName &name) const
{
    if (name == "Global Scroll") return PropertyContainer::ToggleProperty;
    if (name == "Global Zoom") return PropertyContainer::ToggleProperty;
    if (name == "Follow Playback") return PropertyContainer::ValueProperty;
    return PropertyContainer::InvalidProperty;
}

int
View::getPropertyRangeAndValue(const PropertyContainer::PropertyName &name,
			       int *min, int *max, int *deflt) const
{
    if (deflt) *deflt = 1;
    if (name == "Global Scroll") return m_followPan;
    if (name == "Global Zoom") return m_followZoom;
    if (name == "Follow Playback") {
	if (min) *min = 0;
	if (max) *max = 2;
        if (deflt) *deflt = int(PlaybackScrollPage);
	return int(m_followPlay);
    }
    if (min) *min = 0;
    if (max) *max = 0;
    if (deflt) *deflt = 0;
    return 0;
}

QString
View::getPropertyValueLabel(const PropertyContainer::PropertyName &name,
			    int value) const
{
    if (name == "Follow Playback") {
	switch (value) {
	default:
	case 0: return tr("Scroll");
	case 1: return tr("Page");
	case 2: return tr("Off");
	}
    }
    return tr("<unknown>");
}

void
View::setProperty(const PropertyContainer::PropertyName &name, int value)
{
    if (name == "Global Scroll") {
	setFollowGlobalPan(value != 0);
    } else if (name == "Global Zoom") {
	setFollowGlobalZoom(value != 0);
    } else if (name == "Follow Playback") {
	switch (value) {
	default:
	case 0: setPlaybackFollow(PlaybackScrollContinuous); break;
	case 1: setPlaybackFollow(PlaybackScrollPage); break;
	case 2: setPlaybackFollow(PlaybackIgnore); break;
	}
    }
}

size_t
View::getPropertyContainerCount() const
{
    return m_layers.size() + 1; // the 1 is for me
}

const PropertyContainer *
View::getPropertyContainer(size_t i) const
{
    return (const PropertyContainer *)(((View *)this)->
				       getPropertyContainer(i));
}

PropertyContainer *
View::getPropertyContainer(size_t i)
{
    if (i == 0) return m_propertyContainer;
    return m_layers[i-1];
}

bool
View::getValueExtents(QString unit, float &min, float &max, bool &log) const
{
    bool have = false;

    for (LayerList::const_iterator i = m_layers.begin();
         i != m_layers.end(); ++i) { 

        QString layerUnit;
        float layerMin = 0.0, layerMax = 0.0;
        float displayMin = 0.0, displayMax = 0.0;
        bool layerLog = false;

        if ((*i)->getValueExtents(layerMin, layerMax, layerLog, layerUnit) &&
            layerUnit.toLower() == unit.toLower()) {

            if ((*i)->getDisplayExtents(displayMin, displayMax)) {

                min = displayMin;
                max = displayMax;
                log = layerLog;
                have = true;
                break;

            } else {

                if (!have || layerMin < min) min = layerMin;
                if (!have || layerMax > max) max = layerMax;
                if (layerLog) log = true;
                have = true;
            }
        }
    }

    return have;
}

int
View::getTextLabelHeight(const Layer *layer, QPainter &paint) const
{
    std::map<int, Layer *> sortedLayers;

    for (LayerList::const_iterator i = m_layers.begin();
         i != m_layers.end(); ++i) { 
        if ((*i)->needsTextLabelHeight()) {
            sortedLayers[getObjectExportId(*i)] = *i;
        }
    }

    int y = 15 + paint.fontMetrics().ascent();

    for (std::map<int, Layer *>::const_iterator i = sortedLayers.begin();
         i != sortedLayers.end(); ++i) {
        if (i->second == layer) return y;
        y += paint.fontMetrics().height();
    }

    return y;
}

void
View::propertyContainerSelected(View *client, PropertyContainer *pc)
{
    if (client != this) return;
    
    if (pc == m_propertyContainer) {
	if (m_haveSelectedLayer) {
	    m_haveSelectedLayer = false;
	    update();
	}
	return;
    }

    delete m_cache;
    m_cache = 0;

    Layer *selectedLayer = 0;

    for (LayerList::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if (*i == pc) {
	    selectedLayer = *i;
	    m_layers.erase(i);
	    break;
	}
    }

    if (selectedLayer) {
	m_haveSelectedLayer = true;
	m_layers.push_back(selectedLayer);
	update();
    } else {
	m_haveSelectedLayer = false;
    }
}

void
View::toolModeChanged()
{
//    std::cerr << "View::toolModeChanged(" << m_manager->getToolMode() << ")" << std::endl;
}

void
View::overlayModeChanged()
{
    delete m_cache;
    m_cache = 0;
    update();
}

void
View::zoomWheelsEnabledChanged()
{
    // subclass might override this
}

long
View::getStartFrame() const
{
    size_t w2 = (width() / 2) * m_zoomLevel;
    size_t frame = m_centreFrame;
    if (frame >= w2) {
	frame -= w2;
	return (frame / m_zoomLevel * m_zoomLevel);
    } else {
	frame = w2 - frame;
	frame = frame / m_zoomLevel * m_zoomLevel;
	return -(long)frame - m_zoomLevel;
    }
}

size_t
View::getEndFrame() const
{
    return getFrameForX(width()) - 1;
}

void
View::setStartFrame(long f)
{
    setCentreFrame(f + m_zoomLevel * (width() / 2));
}

bool
View::setCentreFrame(size_t f, bool e)
{
    bool changeVisible = false;

    if (m_centreFrame != f) {

	int formerPixel = m_centreFrame / m_zoomLevel;

	m_centreFrame = f;

	int newPixel = m_centreFrame / m_zoomLevel;
	
	if (newPixel != formerPixel) {

#ifdef DEBUG_VIEW_WIDGET_PAINT
	    std::cout << "View(" << this << ")::setCentreFrame: newPixel " << newPixel << ", formerPixel " << formerPixel << std::endl;
#endif
	    update();

	    changeVisible = true;
	}

	if (e) emit centreFrameChanged(f, m_followPan, m_followPlay);
    }

    return changeVisible;
}

int
View::getXForFrame(long frame) const
{
    return (frame - getStartFrame()) / m_zoomLevel;
}

long
View::getFrameForX(int x) const
{
    return (long(x) * long(m_zoomLevel)) + getStartFrame();
}

float
View::getYForFrequency(float frequency,
		       float minf,
		       float maxf, 
		       bool logarithmic) const
{
    int h = height();

    if (logarithmic) {

	static float lastminf = 0.0, lastmaxf = 0.0;
	static float logminf = 0.0, logmaxf = 0.0;

	if (lastminf != minf) {
	    lastminf = (minf == 0.0 ? 1.0 : minf);
	    logminf = log10f(minf);
	}
	if (lastmaxf != maxf) {
	    lastmaxf = (maxf < lastminf ? lastminf : maxf);
	    logmaxf = log10f(maxf);
	}

	if (logminf == logmaxf) return 0;
	return h - (h * (log10f(frequency) - logminf)) / (logmaxf - logminf);

    } else {
	
	if (minf == maxf) return 0;
	return h - (h * (frequency - minf)) / (maxf - minf);
    }
}

float
View::getFrequencyForY(int y,
		       float minf,
		       float maxf,
		       bool logarithmic) const
{
    int h = height();

    if (logarithmic) {

	static float lastminf = 0.0, lastmaxf = 0.0;
	static float logminf = 0.0, logmaxf = 0.0;

	if (lastminf != minf) {
	    lastminf = (minf == 0.0 ? 1.0 : minf);
	    logminf = log10f(minf);
	}
	if (lastmaxf != maxf) {
	    lastmaxf = (maxf < lastminf ? lastminf : maxf);
	    logmaxf = log10f(maxf);
	}

	if (logminf == logmaxf) return 0;
	return pow(10.f, logminf + ((logmaxf - logminf) * (h - y)) / h);

    } else {

	if (minf == maxf) return 0;
	return minf + ((h - y) * (maxf - minf)) / h;
    }
}

int
View::getZoomLevel() const
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
//	std::cout << "zoom level: " << m_zoomLevel << std::endl;
#endif
    return m_zoomLevel;
}

void
View::setZoomLevel(size_t z)
{
    if (m_zoomLevel != int(z)) {
	m_zoomLevel = z;
	emit zoomLevelChanged(z, m_followZoom);
	update();
    }
}

bool
View::hasLightBackground() const
{
    for (LayerList::const_iterator i = m_layers.begin();
         i != m_layers.end(); ++i) {
        if (!(*i)->hasLightBackground()) return false;
    }
    return true;
}

View::LayerProgressBar::LayerProgressBar(QWidget *parent) :
    QProgressBar(parent)
{
    QFont f(font());
    f.setPointSize(f.pointSize() * 8 / 10);
    setFont(f);
}

void
View::addLayer(Layer *layer)
{
    delete m_cache;
    m_cache = 0;

    m_layers.push_back(layer);

    m_progressBars[layer] = new LayerProgressBar(this);
    m_progressBars[layer]->setMinimum(0);
    m_progressBars[layer]->setMaximum(100);
    m_progressBars[layer]->setMinimumWidth(80);
    m_progressBars[layer]->hide();
    
    connect(layer, SIGNAL(layerParametersChanged()),
	    this,    SLOT(layerParametersChanged()));
    connect(layer, SIGNAL(layerParameterRangesChanged()),
	    this,    SLOT(layerParameterRangesChanged()));
    connect(layer, SIGNAL(layerMeasurementRectsChanged()),
	    this,    SLOT(layerMeasurementRectsChanged()));
    connect(layer, SIGNAL(layerNameChanged()),
	    this,    SLOT(layerNameChanged()));
    connect(layer, SIGNAL(modelChanged()),
	    this,    SLOT(modelChanged()));
    connect(layer, SIGNAL(modelCompletionChanged()),
	    this,    SLOT(modelCompletionChanged()));
    connect(layer, SIGNAL(modelChanged(size_t, size_t)),
	    this,    SLOT(modelChanged(size_t, size_t)));
    connect(layer, SIGNAL(modelReplaced()),
	    this,    SLOT(modelReplaced()));

    update();

    emit propertyContainerAdded(layer);
}

void
View::removeLayer(Layer *layer)
{
    if (m_deleting) {
	return;
    }

    delete m_cache;
    m_cache = 0;

    for (LayerList::iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if (*i == layer) {
	    m_layers.erase(i);
	    if (m_progressBars.find(layer) != m_progressBars.end()) {
		delete m_progressBars[layer];
		m_progressBars.erase(layer);
	    }
	    break;
	}
    }
    
    disconnect(layer, SIGNAL(layerParametersChanged()),
               this,    SLOT(layerParametersChanged()));
    disconnect(layer, SIGNAL(layerParameterRangesChanged()),
               this,    SLOT(layerParameterRangesChanged()));
    disconnect(layer, SIGNAL(layerNameChanged()),
               this,    SLOT(layerNameChanged()));
    disconnect(layer, SIGNAL(modelChanged()),
               this,    SLOT(modelChanged()));
    disconnect(layer, SIGNAL(modelCompletionChanged()),
               this,    SLOT(modelCompletionChanged()));
    disconnect(layer, SIGNAL(modelChanged(size_t, size_t)),
               this,    SLOT(modelChanged(size_t, size_t)));
    disconnect(layer, SIGNAL(modelReplaced()),
               this,    SLOT(modelReplaced()));

    update();

    emit propertyContainerRemoved(layer);
}

Layer *
View::getSelectedLayer()
{
    if (m_haveSelectedLayer && !m_layers.empty()) {
	return getLayer(getLayerCount() - 1);
    } else {
	return 0;
    }
}

const Layer *
View::getSelectedLayer() const
{
    return const_cast<const Layer *>(const_cast<View *>(this)->getSelectedLayer());
}

void
View::setViewManager(ViewManager *manager)
{
    if (m_manager) {
	m_manager->disconnect(this, SLOT(globalCentreFrameChanged(unsigned long)));
	m_manager->disconnect(this, SLOT(viewCentreFrameChanged(View *, unsigned long)));
	m_manager->disconnect(this, SLOT(viewManagerPlaybackFrameChanged(unsigned long)));
	m_manager->disconnect(this, SLOT(viewZoomLevelChanged(View *, unsigned long, bool)));
        m_manager->disconnect(this, SLOT(toolModeChanged()));
        m_manager->disconnect(this, SLOT(selectionChanged()));
        m_manager->disconnect(this, SLOT(overlayModeChanged()));
        m_manager->disconnect(this, SLOT(zoomWheelsEnabledChanged()));
        disconnect(m_manager, SLOT(viewCentreFrameChanged(unsigned long, bool, PlaybackFollowMode)));
	disconnect(m_manager, SLOT(zoomLevelChanged(unsigned long, bool)));
    }

    m_manager = manager;

    connect(m_manager, SIGNAL(globalCentreFrameChanged(unsigned long)),
	    this, SLOT(globalCentreFrameChanged(unsigned long)));
    connect(m_manager, SIGNAL(viewCentreFrameChanged(View *, unsigned long)),
	    this, SLOT(viewCentreFrameChanged(View *, unsigned long)));
    connect(m_manager, SIGNAL(playbackFrameChanged(unsigned long)),
	    this, SLOT(viewManagerPlaybackFrameChanged(unsigned long)));

    connect(m_manager, SIGNAL(viewZoomLevelChanged(View *, unsigned long, bool)),
	    this, SLOT(viewZoomLevelChanged(View *, unsigned long, bool)));

    connect(m_manager, SIGNAL(toolModeChanged()),
	    this, SLOT(toolModeChanged()));
    connect(m_manager, SIGNAL(selectionChanged()),
	    this, SLOT(selectionChanged()));
    connect(m_manager, SIGNAL(inProgressSelectionChanged()),
	    this, SLOT(selectionChanged()));
    connect(m_manager, SIGNAL(overlayModeChanged()),
            this, SLOT(overlayModeChanged()));
    connect(m_manager, SIGNAL(zoomWheelsEnabledChanged()),
            this, SLOT(zoomWheelsEnabledChanged()));

    connect(this, SIGNAL(centreFrameChanged(unsigned long, bool,
                                            PlaybackFollowMode)),
            m_manager, SLOT(viewCentreFrameChanged(unsigned long, bool,
                                                   PlaybackFollowMode)));

    connect(this, SIGNAL(zoomLevelChanged(unsigned long, bool)),
	    m_manager, SLOT(viewZoomLevelChanged(unsigned long, bool)));

    if (m_followPlay != PlaybackIgnore) {
//        std::cerr << "View::setViewManager: setting centre frame to playback frame: " << m_manager->getPlaybackFrame() << std::endl;
        setCentreFrame(m_manager->getPlaybackFrame(), false);
    } else if (m_followPan) {
//        std::cerr << "View::setViewManager: setting centre frame to global centre frame: " << m_manager->getGlobalCentreFrame() << std::endl;
        setCentreFrame(m_manager->getGlobalCentreFrame(), false);
    }
    if (m_followZoom) setZoomLevel(m_manager->getGlobalZoom());

    toolModeChanged();
}

void
View::setFollowGlobalPan(bool f)
{
    m_followPan = f;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::setFollowGlobalZoom(bool f)
{
    m_followZoom = f;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::drawVisibleText(QPainter &paint, int x, int y, QString text, TextStyle style) const
{
    if (style == OutlinedText) {

	QColor origPenColour = paint.pen().color();
	QColor penColour = origPenColour;
	QColor surroundColour = Qt::white;  //palette().background().color();

	if (!hasLightBackground()) {
	    int h, s, v;
	    penColour.getHsv(&h, &s, &v);
	    penColour = QColor::fromHsv(h, s, 255 - v);
	    surroundColour = Qt::black;
	}

	paint.setPen(surroundColour);

	for (int dx = -1; dx <= 1; ++dx) {
	    for (int dy = -1; dy <= 1; ++dy) {
		if (!(dx || dy)) continue;
		paint.drawText(x + dx, y + dy, text);
	    }
	}

	paint.setPen(penColour);

	paint.drawText(x, y, text);

	paint.setPen(origPenColour);

    } else {

	std::cerr << "ERROR: View::drawVisibleText: Boxed style not yet implemented!" << std::endl;
    }
}

void
View::setPlaybackFollow(PlaybackFollowMode m)
{
    m_followPlay = m;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::modelChanged()
{
    QObject *obj = sender();

#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr << "View(" << this << ")::modelChanged()" << std::endl;
#endif
    
    // If the model that has changed is not used by any of the cached
    // layers, we won't need to recreate the cache
    
    bool recreate = false;

    bool discard;
    LayerList scrollables = getScrollableBackLayers(false, discard);
    for (LayerList::const_iterator i = scrollables.begin();
	 i != scrollables.end(); ++i) {
	if (*i == obj || (*i)->getModel() == obj) {
	    recreate = true;
	    break;
	}
    }

    if (recreate) {
	delete m_cache;
	m_cache = 0;
    }

    checkProgress(obj);

    update();
}

void
View::modelChanged(size_t startFrame, size_t endFrame)
{
    QObject *obj = sender();

    long myStartFrame = getStartFrame();
    size_t myEndFrame = getEndFrame();

#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr << "View(" << this << ")::modelChanged(" << startFrame << "," << endFrame << ") [me " << myStartFrame << "," << myEndFrame << "]" << std::endl;
#endif

    if (myStartFrame > 0 && endFrame < size_t(myStartFrame)) {
	checkProgress(obj);
	return;
    }
    if (startFrame > myEndFrame) {
	checkProgress(obj);
	return;
    }

    // If the model that has changed is not used by any of the cached
    // layers, we won't need to recreate the cache
    
    bool recreate = false;

    bool discard;
    LayerList scrollables = getScrollableBackLayers(false, discard);
    for (LayerList::const_iterator i = scrollables.begin();
	 i != scrollables.end(); ++i) {
	if (*i == obj || (*i)->getModel() == obj) {
	    recreate = true;
	    break;
	}
    }

    if (recreate) {
	delete m_cache;
	m_cache = 0;
    }

    if (long(startFrame) < myStartFrame) startFrame = myStartFrame;
    if (endFrame > myEndFrame) endFrame = myEndFrame;

    checkProgress(obj);

    update();
}    

void
View::modelCompletionChanged()
{
    QObject *obj = sender();
    checkProgress(obj);
}

void
View::modelReplaced()
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr << "View(" << this << ")::modelReplaced()" << std::endl;
#endif
    delete m_cache;
    m_cache = 0;

    update();
}

void
View::layerParametersChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());

#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr << "View::layerParametersChanged()" << std::endl;
#endif

    delete m_cache;
    m_cache = 0;
    update();

    if (layer) {
	emit propertyContainerPropertyChanged(layer);
    }
}

void
View::layerParameterRangesChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) emit propertyContainerPropertyRangeChanged(layer);
}

void
View::layerMeasurementRectsChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) update();
}

void
View::layerNameChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) emit propertyContainerNameChanged(layer);
}

void
View::globalCentreFrameChanged(unsigned long f)
{
    if (m_followPan) {
        setCentreFrame(f, false);
    }
}

void
View::viewCentreFrameChanged(View *, unsigned long )
{
    // We do nothing with this, but a subclass might
}

void
View::viewManagerPlaybackFrameChanged(unsigned long f)
{
    if (m_manager) {
	if (sender() != m_manager) return;
    }

    if (m_playPointerFrame == f) return;
    bool visible = (getXForFrame(m_playPointerFrame) != getXForFrame(f));
    size_t oldPlayPointerFrame = m_playPointerFrame;
    m_playPointerFrame = f;
    if (!visible) return;

    switch (m_followPlay) {

    case PlaybackScrollContinuous:
	if (QApplication::mouseButtons() == Qt::NoButton) {
	    setCentreFrame(f, false);
	}
	break;

    case PlaybackScrollPage:
    { 
	int xold = getXForFrame(oldPlayPointerFrame);
	update(xold - 1, 0, 3, height());

	long w = getEndFrame() - getStartFrame();
	w -= w/5;
	long sf = (f / w) * w - w/8;

	if (m_manager &&
	    m_manager->isPlaying() &&
	    m_manager->getPlaySelectionMode()) {
	    MultiSelection::SelectionList selections = m_manager->getSelections();
	    if (!selections.empty()) {
		size_t selectionStart = selections.begin()->getStartFrame();
		if (sf < long(selectionStart) - w / 10) {
		    sf = long(selectionStart) - w / 10;
		}
	    }
	}

#ifdef DEBUG_VIEW_WIDGET_PAINT
	std::cerr << "PlaybackScrollPage: f = " << f << ", sf = " << sf << ", start frame "
		  << getStartFrame() << std::endl;
#endif

	// We don't consider scrolling unless the pointer is outside
	// the clearly visible range already

	int xnew = getXForFrame(m_playPointerFrame);

#ifdef DEBUG_VIEW_WIDGET_PAINT
	std::cerr << "xnew = " << xnew << ", width = " << width() << std::endl;
#endif

	if (xnew < width()/8 || xnew > (width()*7)/8) {
	    if (QApplication::mouseButtons() == Qt::NoButton) {
		long offset = getFrameForX(width()/2) - getStartFrame();
		long newCentre = sf + offset;
		bool changed = setCentreFrame(newCentre, false);
		if (changed) {
		    xold = getXForFrame(oldPlayPointerFrame);
		    update(xold - 1, 0, 3, height());
		}
	    }
	}

	update(xnew - 1, 0, 3, height());

	break;
    }

    case PlaybackIgnore:
	if (long(f) >= getStartFrame() && f < getEndFrame()) {
	    update();
	}
	break;
    }
}

void
View::viewZoomLevelChanged(View *p, unsigned long z, bool locked)
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr  << "View[" << this << "]: viewZoomLevelChanged(" << p << ", " << z << ", " << locked << ")" << std::endl;
#endif
    if (m_followZoom && p != this && locked) {
        setZoomLevel(z);
    }
}

void
View::selectionChanged()
{
    if (m_selectionCached) {
	delete m_cache;
	m_cache = 0;
	m_selectionCached = false;
    }
    update();
}

size_t
View::getFirstVisibleFrame() const
{
    long f0 = getStartFrame();
    size_t f = getModelsStartFrame();
    if (f0 < 0 || f0 < long(f)) return f;
    return f0;
}

size_t 
View::getLastVisibleFrame() const
{
    size_t f0 = getEndFrame();
    size_t f = getModelsEndFrame();
    if (f0 > f) return f;
    return f0;
}

size_t
View::getModelsStartFrame() const
{
    bool first = true;
    size_t startFrame = 0;

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {

	if ((*i)->getModel() && (*i)->getModel()->isOK()) {

	    size_t thisStartFrame = (*i)->getModel()->getStartFrame();

	    if (first || thisStartFrame < startFrame) {
		startFrame = thisStartFrame;
	    }
	    first = false;
	}
    }
    return startFrame;
}

size_t
View::getModelsEndFrame() const
{
    bool first = true;
    size_t endFrame = 0;

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {

	if ((*i)->getModel() && (*i)->getModel()->isOK()) {

	    size_t thisEndFrame = (*i)->getModel()->getEndFrame();

	    if (first || thisEndFrame > endFrame) {
		endFrame = thisEndFrame;
	    }
	    first = false;
	}
    }

    if (first) return getModelsStartFrame();
    return endFrame;
}

int
View::getModelsSampleRate() const
{
    //!!! Just go for the first, for now.  If we were supporting
    // multiple samplerates, we'd probably want to do frame/time
    // conversion in the model

    //!!! nah, this wants to always return the sr of the main model!

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if ((*i)->getModel() && (*i)->getModel()->isOK()) {
	    return (*i)->getModel()->getSampleRate();
	}
    }
    return 0;
}

bool
View::areLayersScrollable() const
{
    // True iff all views are scrollable
    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if (!(*i)->isLayerScrollable(this)) return false;
    }
    return true;
}

View::LayerList
View::getScrollableBackLayers(bool testChanged, bool &changed) const
{
    changed = false;

    // We want a list of all the scrollable layers that are behind the
    // backmost non-scrollable layer.

    LayerList scrollables;
    bool metUnscrollable = false;

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
//        std::cerr << "View::getScrollableBackLayers: calling isLayerDormant on layer " << *i << std::endl;
//        std::cerr << "(name is " << (*i)->objectName().toStdString() << ")"
//                  << std::endl;
//        std::cerr << "View::getScrollableBackLayers: I am " << this << std::endl;
	if ((*i)->isLayerDormant(this)) continue;
	if ((*i)->isLayerOpaque()) {
	    // You can't see anything behind an opaque layer!
	    scrollables.clear();
            if (metUnscrollable) break;
	}
	if (!metUnscrollable && (*i)->isLayerScrollable(this)) {
            scrollables.push_back(*i);
        } else {
            metUnscrollable = true;
        }
    }

    if (testChanged && scrollables != m_lastScrollableBackLayers) {
	m_lastScrollableBackLayers = scrollables;
	changed = true;
    }
    return scrollables;
}

View::LayerList
View::getNonScrollableFrontLayers(bool testChanged, bool &changed) const
{
    changed = false;
    LayerList nonScrollables;

    // Everything in front of the first non-scrollable from the back
    // should also be considered non-scrollable

    bool started = false;

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if ((*i)->isLayerDormant(this)) continue;
	if (!started && (*i)->isLayerScrollable(this)) {
	    continue;
	}
	started = true;
	if ((*i)->isLayerOpaque()) {
	    // You can't see anything behind an opaque layer!
	    nonScrollables.clear();
	}
	nonScrollables.push_back(*i);
    }

    if (testChanged && nonScrollables != m_lastNonScrollableBackLayers) {
	m_lastNonScrollableBackLayers = nonScrollables;
	changed = true;
    }

    return nonScrollables;
}

size_t
View::getZoomConstraintBlockSize(size_t blockSize,
				 ZoomConstraint::RoundingDirection dir)
    const
{
    size_t candidate = blockSize;
    bool haveCandidate = false;

    PowerOfSqrtTwoZoomConstraint defaultZoomConstraint;

    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {

	const ZoomConstraint *zoomConstraint = (*i)->getZoomConstraint();
	if (!zoomConstraint) zoomConstraint = &defaultZoomConstraint;

	size_t thisBlockSize =
	    zoomConstraint->getNearestBlockSize(blockSize, dir);

	// Go for the block size that's furthest from the one
	// passed in.  Most of the time, that's what we want.
	if (!haveCandidate ||
	    (thisBlockSize > blockSize && thisBlockSize > candidate) ||
	    (thisBlockSize < blockSize && thisBlockSize < candidate)) {
	    candidate = thisBlockSize;
	    haveCandidate = true;
	}
    }

    return candidate;
}

bool
View::areLayerColoursSignificant() const
{
    for (LayerList::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
	if ((*i)->isLayerColourSignificant()) return true;
        if ((*i)->isLayerOpaque()) break;
    }
    return false;
}

bool
View::hasTopLayerTimeXAxis() const
{
    LayerList::const_iterator i = m_layers.end();
    if (i == m_layers.begin()) return false;
    --i;
    return (*i)->hasTimeXAxis();
}

void
View::zoom(bool in)
{
    int newZoomLevel = m_zoomLevel;

    if (in) {
	newZoomLevel = getZoomConstraintBlockSize(newZoomLevel - 1, 
						  ZoomConstraint::RoundDown);
    } else {
	newZoomLevel = getZoomConstraintBlockSize(newZoomLevel + 1,
						  ZoomConstraint::RoundUp);
    }

    if (newZoomLevel != m_zoomLevel) {
	setZoomLevel(newZoomLevel);
    }
}

void
View::scroll(bool right, bool lots)
{
    long delta;
    if (lots) {
	delta = (getEndFrame() - getStartFrame()) / 2;
    } else {
	delta = (getEndFrame() - getStartFrame()) / 20;
    }
    if (right) delta = -delta;

    if (int(m_centreFrame) < delta) {
	setCentreFrame(0);
    } else if (int(m_centreFrame) - delta >= int(getModelsEndFrame())) {
	setCentreFrame(getModelsEndFrame());
    } else {
	setCentreFrame(m_centreFrame - delta);
    }
}

void
View::checkProgress(void *object)
{
    if (!m_showProgress) return;

    int ph = height();

    for (ProgressMap::const_iterator i = m_progressBars.begin();
	 i != m_progressBars.end(); ++i) {

	if (i->first == object) {

	    int completion = i->first->getCompletion(this);

	    if (completion >= 100) {

		i->second->hide();

	    } else {

		i->second->setText(i->first->getPropertyContainerName());

		i->second->setValue(completion);
		i->second->move(0, ph - i->second->height());

		i->second->show();
		i->second->update();

		ph -= i->second->height();
	    }
	} else {
	    if (i->second->isVisible()) {
		ph -= i->second->height();
	    }
	}
    }
}

void
View::paintEvent(QPaintEvent *e)
{
//    Profiler prof("View::paintEvent", false);
//    std::cerr << "View::paintEvent: centre frame is " << m_centreFrame << std::endl;

    if (m_layers.empty()) {
	QFrame::paintEvent(e);
	return;
    }

    // ensure our constraints are met

/*!!! Should we do this only if we have layers that can't support other
  zoom levels?

    m_zoomLevel = getZoomConstraintBlockSize(m_zoomLevel,
					     ZoomConstraint::RoundUp);
*/

    QPainter paint;
    bool repaintCache = false;
    bool paintedCacheRect = false;

    QRect cacheRect(rect());

    if (e) {
	cacheRect &= e->rect();
#ifdef DEBUG_VIEW_WIDGET_PAINT
	std::cerr << "paint rect " << cacheRect.width() << "x" << cacheRect.height()
		  << ", my rect " << width() << "x" << height() << std::endl;
#endif
    }

    QRect nonCacheRect(cacheRect);

    // If not all layers are scrollable, but some of the back layers
    // are, we should store only those in the cache.

    bool layersChanged = false;
    LayerList scrollables = getScrollableBackLayers(true, layersChanged);
    LayerList nonScrollables = getNonScrollableFrontLayers(true, layersChanged);
    bool selectionCacheable = nonScrollables.empty();
    bool haveSelections = m_manager && !m_manager->getSelections().empty();
    bool selectionDrawn = false;

    // If all the non-scrollable layers are non-opaque, then we draw
    // the selection rectangle behind them and cache it.  If any are
    // opaque, however, we can't cache.
    //
    if (!selectionCacheable) {
	selectionCacheable = true;
	for (LayerList::const_iterator i = nonScrollables.begin();
	     i != nonScrollables.end(); ++i) {
	    if ((*i)->isLayerOpaque()) {
		selectionCacheable = false;
		break;
	    }
	}
    }

    if (selectionCacheable) {
	QPoint localPos;
	bool closeToLeft, closeToRight;
	if (shouldIlluminateLocalSelection(localPos, closeToLeft, closeToRight)) {
	    selectionCacheable = false;
	}
    }

#ifdef DEBUG_VIEW_WIDGET_PAINT
    std::cerr << "View(" << this << ")::paintEvent: have " << scrollables.size()
	      << " scrollable back layers and " << nonScrollables.size()
	      << " non-scrollable front layers" << std::endl;
    std::cerr << "haveSelections " << haveSelections << ", selectionCacheable "
	      << selectionCacheable << ", m_selectionCached " << m_selectionCached << std::endl;
#endif

    if (layersChanged || scrollables.empty() ||
	(haveSelections && (selectionCacheable != m_selectionCached))) {
	delete m_cache;
	m_cache = 0;
	m_selectionCached = false;
    }

    if (!scrollables.empty()) {

#ifdef DEBUG_VIEW_WIDGET_PAINT
        std::cerr << "View(" << this << "): cache " << m_cache << ", cache zoom "
                  << m_cacheZoomLevel << ", zoom " << m_zoomLevel << std::endl;
#endif

	if (!m_cache ||
	    m_cacheZoomLevel != m_zoomLevel ||
	    width() != m_cache->width() ||
	    height() != m_cache->height()) {

	    // cache is not valid

	    if (cacheRect.width() < width()/10) {
		delete m_cache;
                m_cache = 0;
#ifdef DEBUG_VIEW_WIDGET_PAINT
		std::cerr << "View(" << this << ")::paintEvent: small repaint, not bothering to recreate cache" << std::endl;
#endif
	    } else {
		delete m_cache;
		m_cache = new QPixmap(width(), height());
#ifdef DEBUG_VIEW_WIDGET_PAINT
		std::cerr << "View(" << this << ")::paintEvent: recreated cache" << std::endl;
#endif
		cacheRect = rect();
		repaintCache = true;
	    }

	} else if (m_cacheCentreFrame != m_centreFrame) {

	    long dx =
		getXForFrame(m_cacheCentreFrame) -
		getXForFrame(m_centreFrame);

	    if (dx > -width() && dx < width()) {
#if defined(Q_WS_WIN32) || defined(Q_WS_MAC)
		// Copying a pixmap to itself doesn't work properly on Windows
		// or Mac (it only works when moving in one direction)
		static QPixmap *tmpPixmap = 0;
		if (!tmpPixmap ||
		    tmpPixmap->width() != width() ||
		    tmpPixmap->height() != height()) {
		    delete tmpPixmap;
		    tmpPixmap = new QPixmap(width(), height());
		}
		paint.begin(tmpPixmap);
		paint.drawPixmap(0, 0, *m_cache);
		paint.end();
		paint.begin(m_cache);
		paint.drawPixmap(dx, 0, *tmpPixmap);
		paint.end();
#else
		// But it seems to be fine on X11
		paint.begin(m_cache);
		paint.drawPixmap(dx, 0, *m_cache);
		paint.end();
#endif

		if (dx < 0) {
		    cacheRect = QRect(width() + dx, 0, -dx, height());
		} else {
		    cacheRect = QRect(0, 0, dx, height());
		}
#ifdef DEBUG_VIEW_WIDGET_PAINT
		std::cerr << "View(" << this << ")::paintEvent: scrolled cache by " << dx << std::endl;
#endif
	    } else {
		cacheRect = rect();
#ifdef DEBUG_VIEW_WIDGET_PAINT
		std::cerr << "View(" << this << ")::paintEvent: scrolling too far" << std::endl;
#endif
	    }
	    repaintCache = true;

	} else {
#ifdef DEBUG_VIEW_WIDGET_PAINT
	    std::cerr << "View(" << this << ")::paintEvent: cache is good" << std::endl;
#endif
	    paint.begin(this);
	    paint.drawPixmap(cacheRect, *m_cache, cacheRect);
	    paint.end();
	    QFrame::paintEvent(e);
	    paintedCacheRect = true;
	}

	m_cacheCentreFrame = m_centreFrame;
	m_cacheZoomLevel = m_zoomLevel;
    }

#ifdef DEBUG_VIEW_WIDGET_PAINT
//    std::cerr << "View(" << this << ")::paintEvent: cacheRect " << cacheRect << ", nonCacheRect " << (nonCacheRect | cacheRect) << ", repaintCache " << repaintCache << ", paintedCacheRect " << paintedCacheRect << std::endl;
#endif

    // Scrollable (cacheable) items first

    if (!paintedCacheRect) {

	if (repaintCache) paint.begin(m_cache);
	else paint.begin(this);

	paint.setClipRect(cacheRect);
	
	if (hasLightBackground()) {
	    paint.setPen(Qt::white);
	    paint.setBrush(Qt::white);
	} else {
	    paint.setPen(Qt::black);
	    paint.setBrush(Qt::black);
	}
	paint.drawRect(cacheRect);

	paint.setPen(Qt::black);
	paint.setBrush(Qt::NoBrush);
	
	for (LayerList::iterator i = scrollables.begin(); i != scrollables.end(); ++i) {
	    paint.setRenderHint(QPainter::Antialiasing, false);
	    paint.save();
	    (*i)->paint(this, paint, cacheRect);
	    paint.restore();
	}

	if (haveSelections && selectionCacheable) {
	    drawSelections(paint);
	    m_selectionCached = repaintCache;
	    selectionDrawn = true;
	}
	
	paint.end();

	if (repaintCache) {
	    cacheRect |= (e ? e->rect() : rect());
	    paint.begin(this);
	    paint.drawPixmap(cacheRect, *m_cache, cacheRect);
	    paint.end();
	}
    }

    // Now non-cacheable items.  We always need to redraw the
    // non-cacheable items across at least the area we drew of the
    // cacheable items.

    nonCacheRect |= cacheRect;

    paint.begin(this);
    paint.setClipRect(nonCacheRect);

    if (scrollables.empty()) {
	if (hasLightBackground()) {
	    paint.setPen(Qt::white);
	    paint.setBrush(Qt::white);
	} else {
	    paint.setPen(Qt::black);
	    paint.setBrush(Qt::black);
	}
	paint.drawRect(nonCacheRect);
    }
	
    paint.setPen(Qt::black);
    paint.setBrush(Qt::NoBrush);
	
    for (LayerList::iterator i = nonScrollables.begin(); i != nonScrollables.end(); ++i) {
//        Profiler profiler2("View::paintEvent non-cacheable");
	(*i)->paint(this, paint, nonCacheRect);
    }
	
    paint.end();

    paint.begin(this);
    if (e) paint.setClipRect(e->rect());
    if (!m_selectionCached) {
	drawSelections(paint);
    }
    paint.end();

    bool showPlayPointer = true;
    if (m_followPlay == PlaybackScrollContinuous) {
        showPlayPointer = false;
    } else if (long(m_playPointerFrame) <= getStartFrame() ||
               m_playPointerFrame >= getEndFrame()) {
        showPlayPointer = false;
    } else if (m_manager && !m_manager->isPlaying()) {
        if (m_playPointerFrame == getCentreFrame() &&
            m_followPlay != PlaybackIgnore) {
            showPlayPointer = false;
        }
    }

    if (showPlayPointer) {

	paint.begin(this);

        int playx = getXForFrame(m_playPointerFrame);
        
        paint.setPen(Qt::black);
        paint.drawLine(playx - 1, 0, playx - 1, height() - 1);
        paint.drawLine(playx + 1, 0, playx + 1, height() - 1);
        paint.drawPoint(playx, 0);
        paint.drawPoint(playx, height() - 1);
        paint.setPen(Qt::white);
        paint.drawLine(playx, 1, playx, height() - 2);

	paint.end();
    }

    QFrame::paintEvent(e);
}

void
View::drawSelections(QPainter &paint)
{
    if (!hasTopLayerTimeXAxis()) return;

    MultiSelection::SelectionList selections;

    if (m_manager) {
	selections = m_manager->getSelections();
	if (m_manager->haveInProgressSelection()) {
	    bool exclusive;
	    Selection inProgressSelection =
		m_manager->getInProgressSelection(exclusive);
	    if (exclusive) selections.clear();
	    selections.insert(inProgressSelection);
	}
    }

    paint.save();

    bool translucent = !areLayerColoursSignificant();

    if (translucent) {
        paint.setBrush(QColor(150, 150, 255, 80));
    } else {
        paint.setBrush(Qt::NoBrush);
    }

    int sampleRate = getModelsSampleRate();

    QPoint localPos;
    long illuminateFrame = -1;
    bool closeToLeft, closeToRight;

    if (shouldIlluminateLocalSelection(localPos, closeToLeft, closeToRight)) {
	illuminateFrame = getFrameForX(localPos.x());
    }

    const QFontMetrics &metrics = paint.fontMetrics();

    for (MultiSelection::SelectionList::iterator i = selections.begin();
	 i != selections.end(); ++i) {

	int p0 = getXForFrame(i->getStartFrame());
	int p1 = getXForFrame(i->getEndFrame());

	if (p1 < 0 || p0 > width()) continue;

#ifdef DEBUG_VIEW_WIDGET_PAINT
	std::cerr << "View::drawSelections: " << p0 << ",-1 [" << (p1-p0) << "x" << (height()+1) << "]" << std::endl;
#endif

	bool illuminateThis =
	    (illuminateFrame >= 0 && i->contains(illuminateFrame));

	paint.setPen(QColor(150, 150, 255));

        if (translucent && shouldLabelSelections()) {
            paint.drawRect(p0, -1, p1 - p0, height() + 1);
        } else {
            // Make the top & bottom lines of the box visible if we
            // are lacking some of the other visual cues.  There's no
            // particular logic to this, it's just a question of what
            // I happen to think looks nice.
            paint.drawRect(p0, 0, p1 - p0, height() - 1);
        }

	if (illuminateThis) {
	    paint.save();
	    if (hasLightBackground()) {
		paint.setPen(QPen(Qt::black, 2));
	    } else {
		paint.setPen(QPen(Qt::white, 2));
	    }
	    if (closeToLeft) {
		paint.drawLine(p0, 1, p1, 1);
		paint.drawLine(p0, 0, p0, height());
		paint.drawLine(p0, height() - 1, p1, height() - 1);
	    } else if (closeToRight) {
		paint.drawLine(p0, 1, p1, 1);
		paint.drawLine(p1, 0, p1, height());
		paint.drawLine(p0, height() - 1, p1, height() - 1);
	    } else {
		paint.setBrush(Qt::NoBrush);
		paint.drawRect(p0, 1, p1 - p0, height() - 2);
	    }
	    paint.restore();
	}

	if (sampleRate && shouldLabelSelections() && m_manager &&
            m_manager->shouldShowSelectionExtents()) {
	    
	    QString startText = QString("%1 / %2")
		.arg(QString::fromStdString
		     (RealTime::frame2RealTime
		      (i->getStartFrame(), sampleRate).toText(true)))
		.arg(i->getStartFrame());
	    
	    QString endText = QString(" %1 / %2")
		.arg(QString::fromStdString
		     (RealTime::frame2RealTime
		      (i->getEndFrame(), sampleRate).toText(true)))
		.arg(i->getEndFrame());
	    
	    QString durationText = QString("(%1 / %2) ")
		.arg(QString::fromStdString
		     (RealTime::frame2RealTime
		      (i->getEndFrame() - i->getStartFrame(), sampleRate)
		      .toText(true)))
		.arg(i->getEndFrame() - i->getStartFrame());

	    int sw = metrics.width(startText),
		ew = metrics.width(endText),
		dw = metrics.width(durationText);

	    int sy = metrics.ascent() + metrics.height() + 4;
	    int ey = sy;
	    int dy = sy + metrics.height();

	    int sx = p0 + 2;
	    int ex = sx;
	    int dx = sx;

	    if (sw + ew > (p1 - p0)) {
		ey += metrics.height();
		dy += metrics.height();
	    }

	    if (ew < (p1 - p0)) {
		ex = p1 - 2 - ew;
	    }

	    if (dw < (p1 - p0)) {
		dx = p1 - 2 - dw;
	    }

	    paint.drawText(sx, sy, startText);
	    paint.drawText(ex, ey, endText);
	    paint.drawText(dx, dy, durationText);
	}
    }

    paint.restore();
}

void
View::drawMeasurementRect(QPainter &paint, const Layer *topLayer, QRect r,
                          bool focus) const
{
//    std::cerr << "View::drawMeasurementRect(" << r.x() << "," << r.y() << " "
//              << r.width() << "x" << r.height() << ")" << std::endl;

    if (r.x() + r.width() < 0 || r.x() >= width()) return;

    if (r.width() != 0 || r.height() != 0) {
        paint.save();
        if (focus) {
            paint.setPen(Qt::NoPen);
            QColor brushColour(Qt::black);
            brushColour.setAlpha(hasLightBackground() ? 15 : 40);
            paint.setBrush(brushColour);
            if (r.x() > 0) {
                paint.drawRect(0, 0, r.x(), height());
            }
            if (r.x() + r.width() < width()) {
                paint.drawRect(r.x() + r.width(), 0, width()-r.x()-r.width(), height());
            }
            if (r.y() > 0) {
                paint.drawRect(r.x(), 0, r.width(), r.y());
            }
            if (r.y() + r.height() < height()) {
                paint.drawRect(r.x(), r.y() + r.height(), r.width(), height()-r.y()-r.height());
            }
            paint.setBrush(Qt::NoBrush);
        }
        paint.setPen(Qt::green);
        paint.drawRect(r);
        paint.restore();
    } else {
        paint.save();
        paint.setPen(Qt::green);
        paint.drawPoint(r.x(), r.y());
        paint.restore();
    }

    if (!focus) return;

    int fontHeight = paint.fontMetrics().height();
    int fontAscent = paint.fontMetrics().ascent();

    float v0, v1;
    QString u0, u1;
    bool b0 = false, b1 = false;

    QString axs, ays, bxs, bys, dxs, dys;

    int axx, axy, bxx, bxy, dxx, dxy;
    int aw = 0, bw = 0, dw = 0;
    
    int labelCount = 0;

    if ((b0 = topLayer->getXScaleValue(this, r.x(), v0, u0))) {
        axs = QString("%1 %2").arg(v0).arg(u0);
        aw = paint.fontMetrics().width(axs);
        ++labelCount;
    }
        
    if (r.width() > 0) {
        if ((b1 = topLayer->getXScaleValue(this, r.x() + r.width(), v1, u1))) {
            bxs = QString("%1 %2").arg(v1).arg(u1);
            bw = paint.fontMetrics().width(bxs);
        }
    }
        
    if (b0 && b1 && u0 == u1) {
        dxs = QString("(%1 %2)").arg(fabs(v1 - v0)).arg(u1);
        dw = paint.fontMetrics().width(dxs);
    }
    
    b0 = false;
    b1 = false;

    if ((b0 = topLayer->getYScaleValue(this, r.y(), v0, u0))) {
        ays = QString("%1 %2").arg(v0).arg(u0);
        aw = std::max(aw, paint.fontMetrics().width(ays));
        ++labelCount;
    }

    if (r.height() > 0) {
        if ((b1 = topLayer->getYScaleValue(this, r.y() + r.height(), v1, u1))) {
            bys = QString("%1 %2").arg(v1).arg(u1);
            bw = std::max(bw, paint.fontMetrics().width(bys));
        }
    }
    
    if (b0 && b1 && u0 == u1) {
        dys = QString("(%1 %2)").arg(fabs(v1 - v0)).arg(u1);
        dw = std::max(dw, paint.fontMetrics().width(dys));
    }

    int mw = r.width();
    int mh = r.height();

    bool edgeLabelsInside = false;
    bool sizeLabelsInside = false;

    if (mw < std::max(aw, std::max(bw, dw)) + 4) {
        // defaults stand
    } else if (mw < aw + bw + 4) {
        if (mh > fontHeight * labelCount * 3 + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        } else if (mh > fontHeight * labelCount * 2 + 4) {
            edgeLabelsInside = true;
        }
    } else if (mw < aw + bw + dw + 4) {
        if (mh > fontHeight * labelCount * 3 + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        } else if (mh > fontHeight * labelCount + 4) {
            edgeLabelsInside = true;
        }
    } else {
        if (mh > fontHeight * labelCount + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        }
    }

    if (edgeLabelsInside) {

        axx = r.x() + 2;
        axy = r.y() + fontAscent + 2;

        bxx = r.x() + r.width() - bw - 2;
        bxy = r.y() + r.height() - (labelCount-1) * fontHeight - 2;

    } else {

        axx = r.x() - aw - 2;
        axy = r.y() + fontAscent;
        
        bxx = r.x() + r.width() + 2;
        bxy = r.y() + r.height() - (labelCount-1) * fontHeight;
    }

    dxx = r.width()/2 + r.x() - dw/2;

    if (sizeLabelsInside) {

        dxy = r.height()/2 + r.y() - (labelCount * fontHeight)/2 + fontAscent;

    } else {

        dxy = r.y() + r.height() + fontAscent + 2;
    }
    
    if (axs != "") {
        drawVisibleText(paint, axx, axy, axs, OutlinedText);
        axy += fontHeight;
    }
    
    if (ays != "") {
        drawVisibleText(paint, axx, axy, ays, OutlinedText);
        axy += fontHeight;
    }

    if (bxs != "") {
        drawVisibleText(paint, bxx, bxy, bxs, OutlinedText);
        bxy += fontHeight;
    }

    if (bys != "") {
        drawVisibleText(paint, bxx, bxy, bys, OutlinedText);
        bxy += fontHeight;
    }

    if (dxs != "") {
        drawVisibleText(paint, dxx, dxy, dxs, OutlinedText);
        dxy += fontHeight;
    }

    if (dys != "") {
        drawVisibleText(paint, dxx, dxy, dys, OutlinedText);
        dxy += fontHeight;
    }
}

bool
View::render(QPainter &paint, int xorigin, size_t f0, size_t f1)
{
    size_t x0 = f0 / m_zoomLevel;
    size_t x1 = f1 / m_zoomLevel;

    size_t w = x1 - x0;

    size_t origCentreFrame = m_centreFrame;

    bool someLayersIncomplete = false;

    for (LayerList::iterator i = m_layers.begin();
         i != m_layers.end(); ++i) {

        int c = (*i)->getCompletion(this);
        if (c < 100) {
            someLayersIncomplete = true;
            break;
        }
    }

    if (someLayersIncomplete) {

        QProgressDialog progress(tr("Waiting for layers to be ready..."),
                                 tr("Cancel"), 0, 100, this);
        
        int layerCompletion = 0;

        while (layerCompletion < 100) {

            for (LayerList::iterator i = m_layers.begin();
                 i != m_layers.end(); ++i) {

                int c = (*i)->getCompletion(this);
                if (i == m_layers.begin() || c < layerCompletion) {
                    layerCompletion = c;
                }
            }

            if (layerCompletion >= 100) break;

            progress.setValue(layerCompletion);
            qApp->processEvents();
            if (progress.wasCanceled()) {
                update();
                return false;
            }

            usleep(50000);
        }
    }

    QProgressDialog progress(tr("Rendering image..."),
                             tr("Cancel"), 0, w / width(), this);

    for (size_t x = 0; x < w; x += width()) {

        progress.setValue(x / width());
        qApp->processEvents();
        if (progress.wasCanceled()) {
            m_centreFrame = origCentreFrame;
            update();
            return false;
        }

        m_centreFrame = f0 + (x + width()/2) * m_zoomLevel;
        
        QRect chunk(0, 0, width(), height());

	if (hasLightBackground()) {
	    paint.setPen(Qt::white);
	    paint.setBrush(Qt::white);
	} else {
	    paint.setPen(Qt::black);
	    paint.setBrush(Qt::black);
	}

	paint.drawRect(QRect(xorigin + x, 0, width(), height()));

	paint.setPen(Qt::black);
	paint.setBrush(Qt::NoBrush);

	for (LayerList::iterator i = m_layers.begin();
             i != m_layers.end(); ++i) {

	    paint.setRenderHint(QPainter::Antialiasing, false);

	    paint.save();
            paint.translate(xorigin + x, 0);

            std::cerr << "Centre frame now: " << m_centreFrame << " drawing to " << chunk.x() + x + xorigin << ", " << chunk.width() << std::endl;

	    (*i)->paint(this, paint, chunk);

	    paint.restore();
	}
    }

    m_centreFrame = origCentreFrame;
    update();
    return true;
}

QImage *
View::toNewImage()
{
    size_t f0 = getModelsStartFrame();
    size_t f1 = getModelsEndFrame();

    return toNewImage(f0, f1);
}

QImage *
View::toNewImage(size_t f0, size_t f1)
{
    size_t x0 = f0 / getZoomLevel();
    size_t x1 = f1 / getZoomLevel();
    
    QImage *image = new QImage(x1 - x0, height(), QImage::Format_RGB32);

    QPainter *paint = new QPainter(image);
    if (!render(*paint, 0, f0, f1)) {
        delete paint;
        delete image;
        return 0;
    } else {
        delete paint;
        return image;
    }
}

QSize
View::getImageSize()
{
    size_t f0 = getModelsStartFrame();
    size_t f1 = getModelsEndFrame();

    return getImageSize(f0, f1);
}
    
QSize
View::getImageSize(size_t f0, size_t f1)
{
    size_t x0 = f0 / getZoomLevel();
    size_t x1 = f1 / getZoomLevel();

    return QSize(x1 - x0, height());
}

QString
View::toXmlString(QString indent, QString extraAttributes) const
{
    QString s;

    s += indent;

    s += QString("<view "
		 "centre=\"%1\" "
		 "zoom=\"%2\" "
		 "followPan=\"%3\" "
		 "followZoom=\"%4\" "
		 "tracking=\"%5\" "
		 " %6>\n")
	.arg(m_centreFrame)
	.arg(m_zoomLevel)
	.arg(m_followPan)
	.arg(m_followZoom)
	.arg(m_followPlay == PlaybackScrollContinuous ? "scroll" :
	     m_followPlay == PlaybackScrollPage ? "page" : "ignore")
	.arg(extraAttributes);

    for (size_t i = 0; i < m_layers.size(); ++i) {
        bool visible = !m_layers[i]->isLayerDormant(this);
	s += m_layers[i]->toBriefXmlString(indent + "  ",
                                           QString("visible=\"%1\"")
                                           .arg(visible ? "true" : "false"));
    }

    s += indent + "</view>\n";

    return s;
}

ViewPropertyContainer::ViewPropertyContainer(View *v) :
    m_v(v)
{
    connect(m_v, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
	    this, SIGNAL(propertyChanged(PropertyContainer::PropertyName)));
}

