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

#include "LevelPanWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

#include "layer/ColourMapper.h"

#include <iostream>

using std::cerr;
using std::endl;

static const int maxLevel = 5;
static const int maxPan = 2; // range is -maxPan to maxPan

LevelPanWidget::LevelPanWidget(QWidget *parent) :
    QWidget(parent),
    m_level(maxLevel),
    m_pan(0),
    m_editable(true)
{
}

LevelPanWidget::~LevelPanWidget()
{
}

void
LevelPanWidget::setLevel(float level)
{
    m_level = int(round(level * maxLevel));
    if (m_level < 0) m_level = 0;
    if (m_level > maxLevel) m_level = maxLevel;
    update();
}

void
LevelPanWidget::setPan(float pan)
{
    m_pan = int(round(pan * maxPan));
    if (m_pan < -maxPan) m_pan = -maxPan;
    if (m_pan > maxPan) m_pan = maxPan;
    update();
}

void
LevelPanWidget::setEditable(bool editable)
{
    m_editable = editable;
    update();
}

float
LevelPanWidget::getLevel() const
{
    return float(m_level) / float(maxLevel);
}

float
LevelPanWidget::getPan() const
{
    return float(m_pan) / float(maxPan);
}

void
LevelPanWidget::emitLevelChanged()
{
    cerr << "emitting levelChanged(" << getLevel() << ")" << endl;
    emit levelChanged(getLevel());
}

void
LevelPanWidget::emitPanChanged()
{
    cerr << "emitting panChanged(" << getPan() << ")" << endl;
    emit panChanged(getPan());
}

void
LevelPanWidget::mousePressEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
}

void
LevelPanWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_editable) return;
    
    int level, pan;
    toCell(e->pos(), level, pan);
    if (level == m_level && pan == m_pan) {
	return;
    }
    if (level != m_level) {
	m_level = level;
	emitLevelChanged();
    }
    if (pan != m_pan) {
	m_pan = pan;
	emitPanChanged();
    }
    update();
}

void
LevelPanWidget::mouseReleaseEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
}

void
LevelPanWidget::wheelEvent(QWheelEvent *e)
{
    if (e->modifiers() & Qt::ControlModifier) {
	if (e->delta() > 0) {
	    if (m_pan < maxPan) {
		++m_pan;
		emitPanChanged();
		update();
	    }
	} else {
	    if (m_pan > -maxPan) {
		--m_pan;
		emitPanChanged();
		update();
	    }
	}
    } else {
	if (e->delta() > 0) {
	    if (m_level < maxLevel) {
		++m_level;
		emitLevelChanged();
		update();
	    }
	} else {
	    if (m_level > 0) {
		--m_level;
		emitLevelChanged();
		update();
	    }
	}
    }
}

void
LevelPanWidget::toCell(QPointF loc, int &level, int &pan) const
{
    double w = width(), h = height();
    int npan = maxPan * 2 + 1;
    double wcell = w / npan, hcell = h / maxLevel;
    level = int((h - loc.y()) / hcell) + 1;
    if (level < 1) level = 1;
    if (level > maxLevel) level = maxLevel;
    pan = int(loc.x() / wcell) - maxPan;
    if (pan < -maxPan) pan = -maxPan;
    if (pan > maxPan) pan = maxPan;
}

QSizeF
LevelPanWidget::cellSize() const
{
    double w = width(), h = height();
    int npan = maxPan * 2 + 1;
    double wcell = w / npan, hcell = h / maxLevel;
    return QSizeF(wcell, hcell);
}

QPointF
LevelPanWidget::cellCentre(int level, int pan) const
{
    QSizeF cs = cellSize();
    return QPointF(cs.width() * (pan + maxPan) + cs.width() / 2.,
		   height() - cs.height() * level + cs.height() / 2.);
}

QSizeF
LevelPanWidget::cellLightSize() const
{
    double extent = 3. / 4.;
    QSizeF cs = cellSize();
    double m = std::min(cs.width(), cs.height());
    return QSizeF(m * extent, m * extent);
}

QRectF
LevelPanWidget::cellLightRect(int level, int pan) const
{
    QSizeF cls = cellLightSize();
    QPointF cc = cellCentre(level, pan);
    return QRectF(cc.x() - cls.width() / 2., 
		  cc.y() - cls.height() / 2.,
		  cls.width(),
		  cls.height());
}

double
LevelPanWidget::thinLineWidth() const
{
    double tw = ceil(width() / (maxPan * 2. * 10.));
    double th = ceil(height() / (maxLevel * 10.));
    return std::min(th, tw);
}

void
LevelPanWidget::paintEvent(QPaintEvent *)
{
    QPainter paint(this);
    ColourMapper mapper(ColourMapper::Sunset, 0, maxLevel);

    paint.setRenderHint(QPainter::Antialiasing, true);

    QPen pen;

    double thin = thinLineWidth();
    
    pen.setColor(QColor(127, 127, 127, 127));
    pen.setWidthF(cellLightSize().width() + thin);
    pen.setCapStyle(Qt::RoundCap);
    paint.setPen(pen);

    for (int pan = -maxPan; pan <= maxPan; ++pan) {
	paint.drawLine(cellCentre(1, pan), cellCentre(maxLevel, pan));
    }
    
    pen.setColor(Qt::black);
    pen.setWidthF(thin);
    pen.setCapStyle(Qt::FlatCap);
    paint.setPen(pen);
    
    for (int level = 1; level <= m_level; ++level) {
	// level starts at 1 because we handle mute separately
	paint.setBrush(mapper.map(level));
	paint.drawEllipse(cellLightRect(level, m_pan));
    }
}


