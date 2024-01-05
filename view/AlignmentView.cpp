/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2014 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AlignmentView.h"

#include <QPainter>

#include "data/model/SparseOneDimensionalModel.h"

#include "layer/TimeInstantLayer.h"

//#define DEBUG_ALIGNMENT_VIEW 1

using std::vector;
using std::set;

AlignmentView::AlignmentView(QWidget *w) :
    View(w, false),
    m_above(nullptr),
    m_below(nullptr),
    m_reference(nullptr),
    m_leftmostAbove(-1),
    m_rightmostAbove(-1)
{
    setObjectName(tr("AlignmentView"));
}

void
AlignmentView::keyFramesChanged()
{
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::keyFramesChanged" << endl;
#endif
    
    // This is just a notification that we need to rebuild - so all we
    // do here is clear, and rebuild on demand later
    QMutexLocker locker(&m_mapsMutex);
    m_fromAboveMap.clear();
    m_fromReferenceMap.clear();
}

void
AlignmentView::globalCentreFrameChanged(sv_frame_t f)
{
    View::globalCentreFrameChanged(f);
    update();
}

void
AlignmentView::viewCentreFrameChanged(View *v, sv_frame_t f)
{
    View::viewCentreFrameChanged(v, f);
    if (v == m_above) {
        m_centreFrame = f;
        update();
    } else if (v == m_below) {
        update();
    }
}

void
AlignmentView::viewManagerPlaybackFrameChanged(sv_frame_t)
{
    update();
}

void
AlignmentView::viewAboveZoomLevelChanged(ZoomLevel level, bool)
{
    m_zoomLevel = level;
    update();
}

void
AlignmentView::viewBelowZoomLevelChanged(ZoomLevel, bool)
{
    update();
}

void
AlignmentView::setAboveView(View *v)
{
    if (m_above) {
        disconnect(m_above, nullptr, this, nullptr);
    }

    m_above = v;

    if (m_above) {
        connect(m_above,
		SIGNAL(zoomLevelChanged(ZoomLevel, bool)),
                this, 
		SLOT(viewAboveZoomLevelChanged(ZoomLevel, bool)));
        connect(m_above,
                SIGNAL(propertyContainerAdded(PropertyContainer *)),
                this,
                SLOT(keyFramesChanged()));
        connect(m_above,
                SIGNAL(layerModelChanged()),
                this,
                SLOT(keyFramesChanged()));
    }

    keyFramesChanged();
}

void
AlignmentView::setBelowView(View *v)
{
    if (m_below) {
        disconnect(m_below, nullptr, this, nullptr);
    }

    m_below = v;

    if (m_below) {
        connect(m_below,
		SIGNAL(zoomLevelChanged(ZoomLevel, bool)),
                this, 
		SLOT(viewBelowZoomLevelChanged(ZoomLevel, bool)));
        connect(m_below,
                SIGNAL(propertyContainerAdded(PropertyContainer *)),
                this,
                SLOT(keyFramesChanged()));
        connect(m_below,
                SIGNAL(layerModelChanged()),
                this,
                SLOT(keyFramesChanged()));
    }

    keyFramesChanged();
}

void
AlignmentView::setReferenceView(View *view)
{
    m_reference = view;
}

void
AlignmentView::paintEvent(QPaintEvent *)
{
    if (m_above == nullptr || m_below == nullptr || !m_manager) return;
    
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::paintEvent" << endl;
#endif
    
    bool darkPalette = false;
    if (m_manager) darkPalette = m_manager->getGlobalDarkBackground();

    QColor fg, bg;
    if (darkPalette) {
        fg = Qt::gray;
        bg = Qt::black;
    } else {
        fg = Qt::black;
        bg = Qt::gray;
    }

    QPainter paint(this);
    paint.setPen(QPen(fg, 2));
    paint.setBrush(Qt::NoBrush);
    paint.setRenderHint(QPainter::Antialiasing, true);

    paint.fillRect(rect(), bg);

    QMutexLocker locker(&m_mapsMutex);

    if (m_fromAboveMap.empty()) {
        reconnectModels();
        buildMaps();
    }

#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::paintEvent: painting "
           << m_fromAboveMap.size() << " mappings" << endl;
#endif

    int w = width();
    int h = height();

    if (m_leftmostAbove >= 0) {

#ifdef DEBUG_ALIGNMENT_VIEW
        SVCERR << "AlignmentView: m_leftmostAbove = " << m_leftmostAbove
               << ", we have a relationship with the pane above us: showing "
               << "mappings in relation to that" << endl;
#endif

        for (const auto &km: m_fromAboveMap) {

            sv_frame_t af = km.first;
            sv_frame_t bf = km.second;
            
            if (af < m_leftmostAbove) {
#ifdef DEBUG_ALIGNMENT_VIEW
                SVCERR << "AlignmentView: af " << af << " < m_leftmostAbove " << m_leftmostAbove << endl;
#endif
                continue;
            }
            if (af > m_rightmostAbove) {
#ifdef DEBUG_ALIGNMENT_VIEW
                SVCERR << "AlignmentView: af " << af << " > m_rightmostAbove " << m_rightmostAbove << endl;
#endif
                continue;
            }

            int ax = m_above->getXForFrame(af);
            int bx = m_below->getXForFrame(bf);

            if (ax >= 0 || ax < w || bx >= 0 || bx < w) {
                paint.drawLine(ax, 0, bx, h);
            }
        }
    } else if (m_reference != nullptr) {
        // the below has nothing in common with the above: show things
        // in common with the reference instead

#ifdef DEBUG_ALIGNMENT_VIEW
        SVCERR << "AlignmentView: m_leftmostAbove = " << m_leftmostAbove
               << ", we have no relationship with the pane above us: showing "
               << "mappings in relation to the reference instead" << endl;
#endif

        for (const auto &km: m_fromReferenceMap) {
            
            sv_frame_t af = km.first;
            sv_frame_t bf = km.second;

            int ax = m_reference->getXForFrame(af);
            int bx = m_below->getXForFrame(bf);

            if (ax >= 0 || ax < w || bx >= 0 || bx < w) {
                paint.drawLine(ax, 0, bx, h);
            }
        }
    }

    paint.end();
}        

void
AlignmentView::reconnectModels()
{
    vector<ModelId> toConnect { 
        getSalientModel(m_above),
        getSalientModel(m_below)
    };

    for (auto modelId: toConnect) {
        if (auto model = ModelById::get(modelId)) {
            auto referenceId = model->getAlignmentReference();
            if (!referenceId.isNone()) {
                toConnect.push_back(referenceId);
            }
        }
    }

    for (auto modelId: toConnect) {
        if (auto model = ModelById::get(modelId)) {
            auto ptr = model.get();
            disconnect(ptr, 0, this, 0);
            connect(ptr, SIGNAL(modelChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
            connect(ptr, SIGNAL(completionChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
            connect(ptr, SIGNAL(alignmentCompletionChanged(ModelId)),
                    this, SLOT(keyFramesChanged()));
        }
    }
}

void
AlignmentView::buildMaps()
{
#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::buildMaps" << endl;
#endif
    
    sv_frame_t resolution = 1;

    set<sv_frame_t> keyFramesBelow;
    for (auto f: getKeyFrames(m_below, resolution)) {
        keyFramesBelow.insert(f);
    }

    for (sv_frame_t f : keyFramesBelow) {
        sv_frame_t rf = m_below->alignToReference(f);
        m_fromReferenceMap.insert({ rf, f });
    }
    
    vector<sv_frame_t> keyFrames = getKeyFrames(m_above, resolution);

    // These are the most extreme leftward and rightward frames in
    // "above" that have distinct corresponding frames in
    // "below". Anything left of m_leftmostAbove or right of
    // m_rightmostAbove maps effectively off one end or the other of
    // the below view. (They don't actually map off the ends, they
    // just all map to the same first/last destination frame. But we
    // don't want to display their mappings, as they're just noise.)
    m_leftmostAbove = -1;
    m_rightmostAbove = -1;

    sv_frame_t prevAf = -1;
    sv_frame_t prevBf = -1;
    
    for (sv_frame_t af : keyFrames) {

        sv_frame_t rf = m_above->alignToReference(af);
        sv_frame_t bf = m_below->alignFromReference(rf);

        if (prevBf > 0 && bf > prevBf) {
            if (m_leftmostAbove < 0) {
                m_leftmostAbove = prevAf;
            }
            m_rightmostAbove = af;
        }
        prevAf = af;
        prevBf = bf;
        
        bool mappedSomething = false;
        
        if (resolution > 1) {
            if (keyFramesBelow.find(bf) == keyFramesBelow.end()) {

                sv_frame_t af1 = af + resolution;
                sv_frame_t rf1 = m_above->alignToReference(af1);
                sv_frame_t bf1 = m_below->alignFromReference(rf1);

                for (sv_frame_t probe = bf + 1; probe <= bf1; ++probe) {
                    if (keyFramesBelow.find(probe) != keyFramesBelow.end()) {
                        m_fromAboveMap.insert({ af, probe });
                        mappedSomething = true;
                    }
                }
            }
        }

        if (!mappedSomething) {
            m_fromAboveMap.insert({ af, bf });
        }
    }

#ifdef DEBUG_ALIGNMENT_VIEW
    SVCERR << "AlignmentView " << getId() << "::buildMaps: have "
           << m_fromAboveMap.size() << " mappings" << endl;
#endif
}

vector<sv_frame_t>
AlignmentView::getKeyFrames(View *view, sv_frame_t &resolution)
{
    resolution = 1;
    
    if (!view) {
        return getDefaultKeyFrames();
    }

    ModelId m = getSalientModel(view);
    auto model = ModelById::getAs<SparseOneDimensionalModel>(m);
    if (!model) {
        return getDefaultKeyFrames();
    }

    resolution = model->getResolution();
    
    vector<sv_frame_t> keyFrames;

    EventVector pp = model->getAllEvents();
    for (EventVector::const_iterator pi = pp.begin(); pi != pp.end(); ++pi) {
        keyFrames.push_back(pi->getFrame());
    }

    return keyFrames;
}

vector<sv_frame_t>
AlignmentView::getDefaultKeyFrames()
{
    vector<sv_frame_t> keyFrames;
    return keyFrames;

#ifdef NOT_REALLY
    if (!m_above || !m_manager) return keyFrames;

    sv_samplerate_t rate = m_manager->getMainModelSampleRate();
    if (rate == 0) return keyFrames;

    for (sv_frame_t f = m_above->getModelsStartFrame(); 
         f <= m_above->getModelsEndFrame(); 
         f += sv_frame_t(rate * 5 + 0.5)) {
        keyFrames.push_back(f);
    }
    
    return keyFrames;
#endif
}

ModelId
AlignmentView::getSalientModel(View *view)
{
    ModelId m;
    
    // get the topmost such
    for (int i = 0; i < view->getLayerCount(); ++i) {
        if (qobject_cast<TimeInstantLayer *>(view->getLayer(i))) {
            ModelId mm = view->getLayer(i)->getModel();
            if (ModelById::isa<SparseOneDimensionalModel>(mm)) {
                m = mm;
            }
        }
    }

    return m;
}


