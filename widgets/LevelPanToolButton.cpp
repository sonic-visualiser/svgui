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

#include "LevelPanToolButton.h"
#include "LevelPanWidget.h"

#include <QMenu>
#include <QWidgetAction>
#include <QImage>
#include <QMouseEvent>
#include <QStylePainter>
#include <QStyleOptionToolButton>
#include <QMenu>

#include "base/Debug.h"
#include "base/AudioLevel.h"
#include "MenuTitle.h"

#include <iostream>
namespace sv {

using std::cerr;
using std::endl;

LevelPanToolButton::LevelPanToolButton(QWidget *parent) :
    QToolButton(parent),
    m_pixels(32),
    m_pixelsBig(32 * 3),
    m_muted(false),
    m_savedLevel(1.f),
    m_provideContextMenu(true),
    m_lastContextMenu(nullptr)
{
    m_lpw = new LevelPanWidget();

    connect(m_lpw, SIGNAL(levelChanged(float)), this, SIGNAL(levelChanged(float)));
    connect(m_lpw, SIGNAL(levelChanged(float)), this, SLOT(selfLevelChanged(float)));

    connect(m_lpw, SIGNAL(panChanged(float)), this, SIGNAL(panChanged(float)));
    connect(m_lpw, SIGNAL(panChanged(float)), this, SLOT(update()));

    connect(this, SIGNAL(clicked(bool)), this, SLOT(selfClicked()));
    
    QMenu *menu = new QMenu();
    QWidgetAction *wa = new QWidgetAction(menu);
    wa->setDefaultWidget(m_lpw);
    menu->addAction(wa);

    setPopupMode(InstantPopup);
    setMenu(menu);
    setToolTip(tr("Click to adjust level and pan"));

    setImageSize(m_pixels);
    setBigImageSize(m_pixelsBig);
    
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)),
            this, SLOT(contextMenuRequested(const QPoint &)));
}

LevelPanToolButton::~LevelPanToolButton()
{
    delete m_lastContextMenu;
}

void
LevelPanToolButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton ||
        ((e->button() == Qt::LeftButton) &&
         (e->modifiers() & Qt::ControlModifier))) {
        m_lpw->setToDefault();
        e->accept();
    } else {
        QToolButton::mousePressEvent(e);
    }
}

void
LevelPanToolButton::wheelEvent(QWheelEvent *e)
{
    m_lpw->wheelEvent(e);
}

float
LevelPanToolButton::getLevel() const
{
    return m_lpw->getLevel();
}

float
LevelPanToolButton::getPan() const
{
    return m_lpw->getPan();
}

bool
LevelPanToolButton::includesMute() const
{
    return m_lpw->includesMute();
}

void
LevelPanToolButton::setImageSize(int pixels)
{
    m_pixels = pixels;

    QPixmap px(m_pixels, m_pixels);
    px.fill(Qt::transparent);
    setIcon(px);
}

void
LevelPanToolButton::setProvideContextMenu(bool provide)
{
    m_provideContextMenu = provide;
}

void
LevelPanToolButton::setBigImageSize(int pixels)
{
    m_pixelsBig = pixels;

    m_lpw->setFixedWidth(m_pixelsBig);
    m_lpw->setFixedHeight(m_pixelsBig);
}

void
LevelPanToolButton::setLevel(float level)
{
    m_lpw->setLevel(level);
    update();
}

void
LevelPanToolButton::setPan(float pan)
{
    m_lpw->setPan(pan);
    update();
}

void
LevelPanToolButton::setMonitoringLevels(float left, float right)
{
    m_lpw->setMonitoringLevels(left, right);
    update();
}

void
LevelPanToolButton::setIncludeMute(bool include)
{
    m_lpw->setIncludeMute(include);
    update();
}

void
LevelPanToolButton::setEnabled(bool enabled)
{
    m_lpw->setEnabled(enabled);
    QToolButton::setEnabled(enabled);
}

void
LevelPanToolButton::selfLevelChanged(float level)
{
    if (level > 0.f) {
        m_muted = false;
    } else {
        m_muted = true;
        m_savedLevel = 1.f;
    }
    update();
}

void
LevelPanToolButton::selfClicked()
{
    if (m_muted) {
        m_muted = false;
        m_lpw->setLevel(m_savedLevel);
        emit levelChanged(m_savedLevel);
    } else {
        m_savedLevel = m_lpw->getLevel();
        m_muted = true;
        m_lpw->setLevel(0.f);
        emit levelChanged(0.f);
    }
    update();
}

void
LevelPanToolButton::contextMenuRequested(const QPoint &pos)
{
    if (!m_provideContextMenu) {
        return;
    }
    
    delete m_lastContextMenu;
    m_lastContextMenu = new QMenu;
    auto m = m_lastContextMenu;

    QString title;

    if (m_muted) {
        title = tr("Muted");
    } else {
        // Pan is actually stereo balance in most applications...
        auto level = AudioLevel::voltage_to_dB(m_lpw->getLevel());
        auto pan = m_lpw->getPan();
        if (pan == 0) {
            title = tr("Level: %1 dB - Balance: Middle").arg(level);
        } else if (pan > 0) {
            title = tr("Level: %1 dB - Balance: +%2").arg(level).arg(pan);
        } else {
            title = tr("Level: %1 dB - Balance: %2").arg(level).arg(pan);
        }
    }
    
    MenuTitle::addTitle(m, title);

    m->addAction(tr("&Reset to Default"), m_lpw, SLOT(setToDefault()));

    m->popup(mapToGlobal(pos));
}

void
LevelPanToolButton::paintEvent(QPaintEvent *)
{
    QStylePainter p(this);
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    opt.features &= (~QStyleOptionToolButton::HasMenu);
    p.drawComplexControl(QStyle::CC_ToolButton, opt);
    
    if (m_pixels >= height()) {
        setImageSize(height()-1);
    }
    
    double margin = (double(height()) - m_pixels) / 2.0;
    m_lpw->renderTo(this, QRectF(margin, margin, m_pixels, m_pixels), false);
}

void
LevelPanToolButton::enterEvent(QEnterEvent *e)
{
    QToolButton::enterEvent(e);
    emit mouseEntered();
}

void
LevelPanToolButton::leaveEvent(QEvent *e)
{
    QToolButton::leaveEvent(e);
    emit mouseLeft();
}


} // end namespace sv

