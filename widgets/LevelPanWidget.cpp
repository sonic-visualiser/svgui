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

/**
 * Gain and pan scales:
 *
 * Gain: we have 5 circles vertically in the display, each of which
 * has half-circle and full-circle versions, and we also have "no
 * circles", so there are in total 11 distinct levels, which we refer
 * to as "notches" and number 0-10. (We use "notch" because "level" is
 * used by the external API to refer to audio gain.)
 * 
 * i.e. the levels are represented by these (schematic, rotated to
 * horizontal) displays:
 *
 *  0  X
 *  1  [
 *  2  []
 *  3  [][
 *  ...
 *  9  [][][][][
 *  10 [][][][][]
 * 
 * If we have mute enabled, then we map the range 0-10 to gain using
 * AudioLevel::fader_to_* with the ShortFader type, which treats fader
 * 0 as muted. If mute is disabled, then we map the range 1-10.
 * 
 * We can also disable half-circles, which leaves the range unchanged
 * but limits the notches to even values.
 * 
 * Pan: we have 5 columns with no finer resolution, so we only have 2
 * possible pan values on each side of centre.
 */

static const int maxPan = 2; // range is -maxPan to maxPan

LevelPanWidget::LevelPanWidget(QWidget *parent) :
    QWidget(parent),
    m_minNotch(0),
    m_maxNotch(10),
    m_notch(m_maxNotch),
    m_pan(0),
    m_monitorLeft(-1),
    m_monitorRight(-1),
    m_editable(true),
    m_editing(false),
    m_includeMute(true),
    m_includeHalfSteps(true),
    m_pendingWheelAngle(0)
{
    setToolTip(tr("Drag vertically to adjust level, horizontally to adjust pan"));
    setLevel(1.0);
    setPan(0.0);
}

LevelPanWidget::~LevelPanWidget()
{
}

void
LevelPanWidget::setToDefault()
{
    setLevel(1.0);
    setPan(0.0);
    emitLevelChanged();
    emitPanChanged();
}

QSize
LevelPanWidget::sizeHint() const
{
    return WidgetScale::scaleQSize(QSize(40, 40));
}

int
LevelPanWidget::clampNotch(int notch) const
{
    if (notch < m_minNotch) notch = m_minNotch;
    if (notch > m_maxNotch) notch = m_maxNotch;
    if (!m_includeHalfSteps) {
        notch = (notch / 2) * 2;
    }
    return notch;
}

int
LevelPanWidget::clampPan(int pan) const
{
    if (pan < -maxPan) pan = -maxPan;
    if (pan > maxPan) pan = maxPan;
    return pan;
}

int
LevelPanWidget::audioLevelToNotch(float audioLevel) const
{
    int notch = AudioLevel::multiplier_to_fader
        (audioLevel, m_maxNotch, AudioLevel::ShortFader);
    return clampNotch(notch);
}

float
LevelPanWidget::notchToAudioLevel(int notch) const
{
    return float(AudioLevel::fader_to_multiplier
                 (notch, m_maxNotch, AudioLevel::ShortFader));
}

void
LevelPanWidget::setLevel(float level)
{
    int notch = audioLevelToNotch(level);
    if (notch != m_notch) {
        m_notch = notch;
        float convertsTo = getLevel();
        if (fabsf(convertsTo - level) > 1e-5) {
            emitLevelChanged();
        }
        update();
    }
    SVCERR << "setLevel: level " << level << " -> notch " << m_notch << " (which converts back to level " << getLevel() << ")" << endl;
}

float
LevelPanWidget::getLevel() const
{
    return notchToAudioLevel(m_notch);
}

int
LevelPanWidget::audioPanToPan(float audioPan) const
{
    int pan = int(round(audioPan * maxPan));
    pan = clampPan(pan);
    return pan;
}

float
LevelPanWidget::panToAudioPan(int pan) const
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
    if (m_includeMute) {
        m_minNotch = 0;
    } else {
        m_minNotch = 1;
    }
    emitLevelChanged();
    update();
}

void
LevelPanWidget::emitLevelChanged()
{
    emit levelChanged(getLevel());
}

void
LevelPanWidget::emitPanChanged()
{
    emit panChanged(getPan());
}

void
LevelPanWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MidButton ||
        ((e->button() == Qt::LeftButton) &&
         (e->modifiers() & Qt::ControlModifier))) {
        setToDefault();
    } else if (e->button() == Qt::LeftButton) {
        m_editing = true;
        mouseMoveEvent(e);
    }
}

void
LevelPanWidget::mouseReleaseEvent(QMouseEvent *e)
{
    mouseMoveEvent(e);
    m_editing = false;
}

void
LevelPanWidget::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_editable) return;
    if (!m_editing) return;
    
    int notch = coordsToNotch(rect(), e->pos());
    int pan = coordsToPan(rect(), e->pos());

    if (notch == m_notch && pan == m_pan) {
        return;
    }
    if (notch != m_notch) {
        m_notch = notch;
        emitLevelChanged();
    }
    if (pan != m_pan) {
        m_pan = pan;
        emitPanChanged();
    }
    update();
}

void
LevelPanWidget::wheelEvent(QWheelEvent *e)
{
    e->accept();
    
    int dy = e->angleDelta().y();
    if (dy == 0) {
        return;
    }

    if (e->phase() == Qt::ScrollBegin ||
        std::abs(dy) >= 120 ||
        (dy > 0 && m_pendingWheelAngle < 0) ||
        (dy < 0 && m_pendingWheelAngle > 0)) {
        m_pendingWheelAngle = dy;
    } else {
        m_pendingWheelAngle += dy;
    }

    if (abs(m_pendingWheelAngle) >= 600) {
        // discard absurd results
        m_pendingWheelAngle = 0;
        return;
    }

    while (abs(m_pendingWheelAngle) >= 120) {

        int sign = (m_pendingWheelAngle < 0 ? -1 : 1);

        if (e->modifiers() & Qt::ControlModifier) {
            m_pan += sign;
            m_pan = clampPan(m_pan);
            emitPanChanged();
            update();
        } else {
            m_notch += sign;
            m_notch = clampNotch(m_notch);
            emitLevelChanged();
            update();
        }

        m_pendingWheelAngle -= sign * 120;
    }
}

int
LevelPanWidget::coordsToNotch(QRectF rect, QPointF loc) const
{
    double h = rect.height();

    int nnotch = m_maxNotch + 1;
    double cell = h / nnotch;

    int notch = int((h - (loc.y() - rect.y())) / cell);
    notch = clampNotch(notch);

    return notch;
}    

int
LevelPanWidget::coordsToPan(QRectF rect, QPointF loc) const
{
    double w = rect.width();

    int npan = maxPan * 2 + 1;
    double cell = w / npan;

    int pan = int((loc.x() - rect.x()) / cell) - maxPan;
    pan = clampPan(pan);

    return pan;
}

QSizeF
LevelPanWidget::cellSize(QRectF rect) const
{
    double w = rect.width(), h = rect.height();
    int ncol = maxPan * 2 + 1;
    int nrow = m_maxNotch/2;
    double wcell = w / ncol, hcell = h / nrow;
    return QSizeF(wcell, hcell);
}

QPointF
LevelPanWidget::cellCentre(QRectF rect, int row, int col) const
{
    QSizeF cs = cellSize(rect);
    return QPointF(rect.x() +
                   cs.width() * (col + maxPan) + cs.width() / 2.,
                   rect.y() + rect.height() -
                   cs.height() * (row + 1) + cs.height() / 2.);
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
LevelPanWidget::cellLightRect(QRectF rect, int row, int col) const
{
    QSizeF cls = cellLightSize(rect);
    QPointF cc = cellCentre(rect, row, col);
    return QRectF(cc.x() - cls.width() / 2., 
                  cc.y() - cls.height() / 2.,
                  cls.width(),
                  cls.height());
}

double
LevelPanWidget::thinLineWidth(QRectF rect) const
{
    double tw = ceil(rect.width() / (maxPan * 2. * 10.));
    double th = ceil(rect.height() / (m_maxNotch/2 * 10.));
    return std::min(th, tw);
}

QRectF
LevelPanWidget::cellOutlineRect(QRectF rect, int row, int col) const
{
    QRectF clr = cellLightRect(rect, row, col);
    double adj = thinLineWidth(rect)/2;
    return clr.adjusted(-adj, -adj, adj, adj);
}

QColor
LevelPanWidget::notchToColour(int notch) const
{
    if (notch < 3) return Qt::black;
    if (notch < 5) return QColor(80, 0, 0);
    if (notch < 7) return QColor(160, 0, 0);
    if (notch < 9) return QColor(255, 0, 0);
    return QColor(255, 255, 0);
}

void
LevelPanWidget::renderTo(QPaintDevice *dev, QRectF rect, bool asIfEditable) const
{
    QPainter paint(dev);

    paint.setRenderHint(QPainter::Antialiasing, true);

    QPen pen;

    double thin = thinLineWidth(rect);

    QColor columnBackground = QColor(180, 180, 180);
    pen.setColor(columnBackground);
    pen.setWidthF(cellLightSize(rect).width() + thin);
    pen.setCapStyle(Qt::RoundCap);
    paint.setPen(pen);
    paint.setBrush(Qt::NoBrush);

    for (int pan = -maxPan; pan <= maxPan; ++pan) {
        paint.drawLine(cellCentre(rect, 0, pan),
                       cellCentre(rect, m_maxNotch/2 - 1, pan));
    }

    bool monitoring = (m_monitorLeft > 0.f || m_monitorRight > 0.f);
    
    if (isEnabled()) {
        pen.setColor(Qt::black);
    } else {
        pen.setColor(Qt::darkGray);
    }

    if (!asIfEditable && m_includeMute && m_notch == 0) {
        // The X for mute takes up the whole display when we're not
        // being rendered in editable style
        pen.setWidthF(thin * 2);
        pen.setCapStyle(Qt::RoundCap);
        paint.setPen(pen);
        paint.drawLine(cellCentre(rect, 0, -maxPan),
                       cellCentre(rect, m_maxNotch/2 - 1, maxPan));
        paint.drawLine(cellCentre(rect, m_maxNotch/2 - 1, -maxPan),
                       cellCentre(rect, 0, maxPan));
    } else {
        // the normal case
        
        // pen a bit less thin than in theory, so that we can erase
        // semi-circles later without leaving a faint edge
        pen.setWidthF(thin * 0.8);
        pen.setCapStyle(Qt::FlatCap);
        paint.setPen(pen);

        if (m_includeMute && m_notch == 0) {
            QRectF clr = cellLightRect(rect, 0, m_pan);
            paint.drawLine(clr.topLeft(), clr.bottomRight());
            paint.drawLine(clr.bottomLeft(), clr.topRight());
        } else {
            for (int notch = 1; notch <= m_notch; notch += 2) {
                if (isEnabled() && !monitoring) {
                    paint.setBrush(notchToColour(notch));
                }
                QRectF clr = cellLightRect(rect, notch/2, m_pan);
                paint.drawEllipse(clr);
            }
            if (m_notch % 2 != 0) {
                QRectF clr = cellOutlineRect(rect, (m_notch-1)/2, m_pan);
                paint.save();
                paint.setPen(Qt::NoPen);
                paint.setBrush(columnBackground);
                paint.drawPie(clr, 0, 180 * 16);
                paint.restore();
            }
        }
    }

    if (monitoring) {
        paint.setPen(Qt::NoPen);
        for (int pan = -maxPan; pan <= maxPan; ++pan) {
            float audioPan = panToAudioPan(pan);
            float audioLevel;
            if (audioPan < 0.f) {
                audioLevel = m_monitorLeft + m_monitorRight * (1.f + audioPan);
            } else {
                audioLevel = m_monitorRight + m_monitorLeft * (1.f - audioPan);
            }
            int notchHere = audioLevelToNotch(audioLevel);
            for (int notch = 1; notch <= notchHere; notch += 2) {
                paint.setBrush(notchToColour(notch));
                QRectF clr = cellLightRect(rect, (notch-1)/2, pan);
                double adj = thinLineWidth(rect)/2;
                clr = clr.adjusted(adj, adj, -adj, -adj);
                if (notch + 2 > notchHere && notchHere % 2 != 0) {
                    paint.drawPie(clr, 180 * 16, 180 * 16);
                } else {
                    paint.drawEllipse(clr);
                }
            }
        }
        paint.setPen(pen);
        paint.setBrush(Qt::NoBrush);
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

