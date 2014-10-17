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

using std::vector;

AlignmentView::AlignmentView(QWidget *w) :
    View(w, false),
    m_above(0),
    m_below(0)
{
    setObjectName(tr("AlignmentView"));
}

void
AlignmentView::globalCentreFrameChanged(int f)
{
    View::globalCentreFrameChanged(f);
    update();
}

void
AlignmentView::viewCentreFrameChanged(View *v, int f)
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
AlignmentView::viewManagerPlaybackFrameChanged(int)
{
    update();
}

void
AlignmentView::viewAboveZoomLevelChanged(int level, bool)
{
    m_zoomLevel = level;
    update();
}

void
AlignmentView::viewBelowZoomLevelChanged(int, bool)
{
    update();
}

void
AlignmentView::setViewAbove(View *v)
{
    if (m_above) {
	disconnect(m_above, 0, this, 0);
    }

    m_above = v;

    if (m_above) {
	connect(m_above,
		SIGNAL(zoomLevelChanged(int, bool)),
		this, 
		SLOT(viewAboveZoomLevelChanged(int, bool)));
    }
}

void
AlignmentView::setViewBelow(View *v)
{
    if (m_below) {
	disconnect(m_below, 0, this, 0);
    }

    m_below = v;

    if (m_below) {
	connect(m_below,
		SIGNAL(zoomLevelChanged(int, bool)),
		this, 
		SLOT(viewBelowZoomLevelChanged(int, bool)));
    }
}

void
AlignmentView::paintEvent(QPaintEvent *)
{
    if (m_above == 0 || m_below == 0 || !m_manager) return;

    int rate = m_manager->getMainModelSampleRate();
    if (rate == 0) return;

    bool darkPalette = false;
    if (m_manager) darkPalette = m_manager->getGlobalDarkBackground();

    QColor fg = Qt::black, bg = Qt::white;
    if (darkPalette) std::swap(fg, bg);

    QPainter paint(this);
    paint.setPen(QPen(fg, 2));
    paint.setBrush(Qt::NoBrush);
    paint.setRenderHint(QPainter::Antialiasing, true);

    paint.fillRect(rect(), bg);

    vector<int> keyFrames;
    for (int f = m_above->getModelsStartFrame(); 
	 f <= m_above->getModelsEndFrame(); 
	 f += rate * 5) {
	keyFrames.push_back(f);
    }

    foreach (int f, keyFrames) {
	int af = m_above->alignFromReference(f);
	int ax = m_above->getXForFrame(af);
	int bf = m_below->alignFromReference(f);
	int bx = m_below->getXForFrame(bf);
	paint.drawLine(ax, 0, bx, height());
    }

    paint.end();
}

