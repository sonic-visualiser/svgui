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

#include <iostream>
using std::cerr;
using std::endl;

LevelPanToolButton::LevelPanToolButton(QWidget *parent) :
    QToolButton(parent),
    m_pixels(32)
{
    m_lpw = new LevelPanWidget();

    connect(m_lpw, SIGNAL(levelChanged(float)), this, SIGNAL(levelChanged(float)));
    connect(m_lpw, SIGNAL(levelChanged(float)), this, SLOT(redraw()));

    connect(m_lpw, SIGNAL(panChanged(float)), this, SIGNAL(panChanged(float)));
    connect(m_lpw, SIGNAL(panChanged(float)), this, SLOT(redraw()));
    
    QMenu *menu = new QMenu();
    QWidgetAction *wa = new QWidgetAction(menu);
    wa->setDefaultWidget(m_lpw);
    menu->addAction(wa);

    setPopupMode(MenuButtonPopup);
    setMenu(menu);
    setCheckable(true);

    redraw();
}

LevelPanToolButton::~LevelPanToolButton()
{
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

void
LevelPanToolButton::setImageSize(int pixels)
{
    m_pixels = pixels;
    redraw();
}

void
LevelPanToolButton::setLevel(float level)
{
    m_lpw->setLevel(level);
}

void
LevelPanToolButton::setPan(float pan)
{
    m_lpw->setPan(pan);
}

void
LevelPanToolButton::redraw()
{
    QSize sz(m_pixels, m_pixels);

    m_lpw->setFixedWidth(m_pixels * 4);
    m_lpw->setFixedHeight(m_pixels * 4);
    
    QPixmap px(sz);
    px.fill(Qt::transparent);
    m_lpw->renderTo(&px, QRectF(QPointF(), sz), false);
    setIcon(px);
}

