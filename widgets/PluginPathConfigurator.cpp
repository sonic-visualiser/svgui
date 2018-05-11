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

#include "PluginPathConfigurator.h"

#include <QPushButton>
#include <QLabel>

#include "IconLoader.h"
#include "WidgetScale.h"

PluginPathConfigurator::PluginPathConfigurator(QWidget *parent) :
    QFrame(parent)
{
    setFrameStyle(StyledPanel | Sunken);
    m_layout = new QGridLayout;
    setLayout(m_layout);
    populate();
}

PluginPathConfigurator::~PluginPathConfigurator()
{
}

void
PluginPathConfigurator::setPath(QStringList directories, QString envVariable)
{
    m_path = directories;
    m_var = envVariable;
    populate();
}

void
PluginPathConfigurator::populate()
{
    QLabel *header = new QLabel;
    header->setText(QString("<b>%1</b>").arg(tr("Location")));
    m_layout->addWidget(header, 0, 3);
    
    for (int i = 0; i < m_path.size(); ++i) {

        int col = 0;
        int row = i + 1;
        QString dir = m_path[i];

        if (i > 0) {
            QPushButton *up = new QPushButton;
            up->setIcon(IconLoader().load("up"));
            up->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
            connect(up, SIGNAL(clicked()), this, SLOT(upClicked()));
            m_layout->addWidget(up, row, col);
        }
        ++col;

        if (i + 1 < m_path.size()) {
            QPushButton *down = new QPushButton;
            down->setIcon(IconLoader().load("down"));
            down->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
            connect(down, SIGNAL(clicked()), this, SLOT(downClicked()));
            m_layout->addWidget(down, row, col);
        }
        ++col;

        QPushButton *del = new QPushButton;
        del->setIcon(IconLoader().load("datadelete"));
        del->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
        connect(del, SIGNAL(clicked()), this, SLOT(deleteClicked()));
        m_layout->addWidget(del, row, col);
        ++col;

        QLabel *dirLabel = new QLabel;
        dirLabel->setText(dir);
        m_layout->addWidget(dirLabel, row, col);
        m_layout->setColumnStretch(col, 10);
        ++col;
    }        
}

void
PluginPathConfigurator::upClicked()
{
    //!!!
}

void
PluginPathConfigurator::downClicked()
{
    //!!!!
}

void
PluginPathConfigurator::deleteClicked()
{
    //!!!
}
