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
#include "base/AudioLevel.h"

#include "WidgetScale.h"

#include <iostream>
#include <cmath>
#include <cassert>

using std::cerr;
using std::endl;

static const int maxLevel = 4; // min is 0, may be mute or not depending on m_includeMute
static const int maxPan = 2; // range is -maxPan to maxPan

LevelPanWidget::LevelPanWidget(QWidget *parent) :
    QWidget(parent),
    m_level(maxLevel),
    m_pan(0),
    m_monitorLeft(-1),
    m_monitorRight(-1),
    m_editable(true),
    m_includeMute(true)
{
}

LevelPanWidget::~LevelPanWidget()
{
}

QSize
LevelPanWidget::sizeHint() const
{
    return WidgetScale::scaleQSize(QSize(40, 40));
}

static int
db_to_level(double db)
{
    // Only if !m_includeMute, otherwise AudioLevel is used.
    // Levels are: +6 0 -6 -12 -20
    assert(maxLevel == 4);
    if (db > 3.) return 4;
    else if (db > -3.) return 3;
    else if (db > -9.) return 2;
    else if (db > -16.) return 1;
    else return 0;
}

static double
level_to_db(int level)
{
    // Only if !m_includeMute, otherwise AudioLevel is used.
    // Levels are: +6 0 -6 -12 -20
    assert(maxLevel == 4);
    if (level >= 4) return 6.;
    else if (level == 3) return 0.;
    else if (level == 2) return -6.;
    else if (level == 1) return -12.;
    else return -20.;
}

int
LevelPanWidget::audioLevelToLevel(float audioLevel, bool withMute)
{
    int level;
    if (withMute) {
        level = AudioLevel::multiplier_to_fader
            (audioLevel, maxLevel, AudioLevel::ShortFader);
    } else {
        level = db_to_level(AudioLevel::multiplier_to_dB(audioLevel));
    }
    if (level < 0) level = 0;
    if (level > maxLevel) level = maxLevel;
    return level;
}

float
LevelPanWidget::levelToAudioLevel(int level, bool withMute)
{
    if (withMute) {
        return float(AudioLevel::fader_to_multiplier
                     (level, maxLevel, AudioLevel::ShortFader));
    } else {
        return float(AudioLevel::dB_to_multiplier(level_to_db(level)));
    }
}

void
LevelPanWidget::setLevel(float flevel)
{
    int level = audioLevelToLevel(flevel, m_includeMute);
    if (level != m_level) {
	m_level = level;
	float convertsTo = getLevel();
	if (fabsf(convertsTo - flevel) > 1e-5) {
	    emitLevelChanged();
	}
	update();
    }
}

float
LevelPanWidget::getLevel() const
{
    return levelToAudioLevel(m_level, m_includeMute);
}

int
LevelPanWidget::audioPanToPan(float audioPan)
{
    int pan = int(round(audioPan * maxPan));
    if (pan < -maxPan) pan = -maxPan;
    if (pan > maxPan) pan = maxPan;
    return pan;
}

float
LevelPanWidget::panToAudioPan(int pan)
{
    return float(pan) / float(maxPan);
}

void
LevelPanWidget::setPan(float fpan)
{
    int pan = audioPanToPan(fpan);
    if (pan != m_pan) {
        m_pan = pan;
        update();
    }
}

float
LevelPanWidget::getPan() const
{
    return panToAudioPan(m_pan);
}

void
LevelPanWidget::setMonitoringLevels(float left, float right)
{
    m_monitorLeft = left;
    m_monitorRight = right;
    update();
}

bool
LevelPanWidget::isEditable() const
{
    return m_editable;
}

bool
LevelPanWidget::includesMute() const
{
    return m_includeMute;
}

void
LevelPanWidget::setEditable(bool editable)
{
    m_editable = editable;
    update();
}

void
LevelPanWidget::setIncludeMute(bool include)
{
    m_includeMute = include;
    emitLevelChanged();
    update();
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
    toCell(rect(), e->pos(), level, pan);
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
LevelPanWidget::toCell(QRectF rect, QPointF loc, int &level, int &pan) const
{
    double w = rect.width(), h = rect.height();

    int npan = maxPan * 2 + 1;
    int nlevel = maxLevel + 1;

    double wcell = w / npan, hcell = h / nlevel;

    level = int((h - (loc.y() - rect.y())) / hcell);
    if (level < 0) level = 0;
    if (level > maxLevel) level = maxLevel;

    pan = int((loc.x() - rect.x()) / wcell) - maxPan;
    if (pan < -maxPan) pan = -maxPan;
    if (pan > maxPan) pan = maxPan;
}

QSizeF
LevelPanWidget::cellSize(QRectF rect) const
{
    double w = rect.width(), h = rect.height();
    int npan = maxPan * 2 + 1;
    int nlevel = maxLevel + 1;
    double wcell = w / npan, hcell = h / nlevel;
    return QSizeF(wcell, hcell);
}

QPointF
LevelPanWidget::cellCentre(QRectF rect, int level, int pan) const
{
    QSizeF cs = cellSize(rect);
    return QPointF(rect.x() + cs.width() * (pan + maxPan) + cs.width() / 2.,
		   rect.y() + rect.height() - cs.height() * (level + 1) + cs.height() / 2.);
}

QSizeF
LevelPanWidget::cellLightSize(QRectF rect) const
{
    double extent = 3. / 4.;
    QSizeF cs = cellSize(rect);
    double m = std::min(cs.width(), cs.height());
    return QSizeF(m * extent, m * extent);
}

QRectF
LevelPanWidget::cellLightRect(QRectF rect, int level, int pan) const
{
    QSizeF cls = cellLightSize(rect);
    QPointF cc = cellCentre(rect, level, pan);
    return QRectF(cc.x() - cls.width() / 2., 
		  cc.y() - cls.height() / 2.,
		  cls.width(),
		  cls.height());
}

double
LevelPanWidget::thinLineWidth(QRectF rect) const
{
    double tw = ceil(rect.width() / (maxPan * 2. * 10.));
    double th = ceil(rect.height() / (maxLevel * 10.));
    return std::min(th, tw);
}

static QColor
level_to_colour(int level)
{
    assert(maxLevel == 4);
    if (level == 0) return Qt::black;
    else if (level == 1) return QColor(80, 0, 0);
    else if (level == 2) return QColor(160, 0, 0);
    else if (level == 3) return QColor(255, 0, 0);
    else return QColor(255, 255, 0);
}

void
LevelPanWidget::renderTo(QPaintDevice *dev, QRectF rect, bool asIfEditable) const
{
    QPainter paint(dev);

    paint.setRenderHint(QPainter::Antialiasing, true);

    QPen pen;

    double thin = thinLineWidth(rect);

    pen.setColor(QColor(127, 127, 127, 127));
    pen.setWidthF(cellLightSize(rect).width() + thin);
    pen.setCapStyle(Qt::RoundCap);
    paint.setPen(pen);
    paint.setBrush(Qt::NoBrush);

    for (int pan = -maxPan; pan <= maxPan; ++pan) {
	paint.drawLine(cellCentre(rect, 0, pan), cellCentre(rect, maxLevel, pan));
    }

    if (m_monitorLeft > 0.f || m_monitorRight > 0.f) {
        paint.setPen(Qt::NoPen);
        for (int pan = -maxPan; pan <= maxPan; ++pan) {
            float audioPan = panToAudioPan(pan);
            float audioLevel;
            if (audioPan < 0.f) {
                audioLevel = m_monitorLeft + m_monitorRight * (1.f + audioPan);
            } else {
                audioLevel = m_monitorRight + m_monitorLeft * (1.f - audioPan);
            }
            int levelHere = audioLevelToLevel(audioLevel, false);
            for (int level = 0; level <= levelHere; ++level) {
                paint.setBrush(level_to_colour(level));
                QRectF clr = cellLightRect(rect, level, pan);
                paint.drawEllipse(clr);
            }
        }
        paint.setPen(pen);
        paint.setBrush(Qt::NoBrush);
    }
    
    if (isEnabled()) {
	pen.setColor(Qt::black);
    } else {
	pen.setColor(Qt::darkGray);
    }

    if (!asIfEditable && m_includeMute && m_level == 0) {
        pen.setWidthF(thin * 2);
        pen.setCapStyle(Qt::RoundCap);
        paint.setPen(pen);
        paint.drawLine(cellCentre(rect, 0, -maxPan),
                       cellCentre(rect, maxLevel, maxPan));
        paint.drawLine(cellCentre(rect, maxLevel, -maxPan),
                       cellCentre(rect, 0, maxPan));
        return;
    }
    
    pen.setWidthF(thin);
    pen.setCapStyle(Qt::FlatCap);
    paint.setPen(pen);
    
    for (int level = 0; level <= m_level; ++level) {
	if (isEnabled()) {
	    paint.setBrush(level_to_colour(level));
	}
	QRectF clr = cellLightRect(rect, level, m_pan);
	if (m_includeMute && m_level == 0) {
	    paint.drawLine(clr.topLeft(), clr.bottomRight());
	    paint.drawLine(clr.bottomLeft(), clr.topRight());
	} else {
	    paint.drawEllipse(clr);
	}
    }
}

void
LevelPanWidget::paintEvent(QPaintEvent *)
{
    renderTo(this, rect(), m_editable);
}

void
LevelPanWidget::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    emit mouseEntered();
}

void
LevelPanWidget::leaveEvent(QEvent *e)
{
    QWidget::enterEvent(e);
    emit mouseLeft();
}

