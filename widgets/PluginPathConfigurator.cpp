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
    QFrame(parent),
    m_innerFrame(0)
{
    setFrameStyle(StyledPanel | Sunken);
    m_layout = new QGridLayout;
    setLayout(m_layout);
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
    delete m_innerFrame;
    m_innerFrame = new QWidget;
    m_layout->addWidget(m_innerFrame, 0, 0);

    QGridLayout *innerLayout = new QGridLayout;
    m_innerFrame->setLayout(innerLayout);
    
    QLabel *header = new QLabel(m_innerFrame);
    header->setText(QString("<b>%1</b>").arg(tr("Location")));
    innerLayout->addWidget(header, 0, 3);
    
    for (int i = 0; i < m_path.size(); ++i) {

        int col = 0;
        int row = i + 1;
        QString dir = m_path[i];

        if (i > 0) {
            QPushButton *up = new QPushButton(m_innerFrame);
            up->setObjectName(QString("%1").arg(i));
            up->setIcon(IconLoader().load("up"));
            up->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
            connect(up, SIGNAL(clicked()), this, SLOT(upClicked()));
            innerLayout->addWidget(up, row, col);
        }
        ++col;

        if (i + 1 < m_path.size()) {
            QPushButton *down = new QPushButton(m_innerFrame);
            down->setObjectName(QString("%1").arg(i));
            down->setIcon(IconLoader().load("down"));
            down->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
            connect(down, SIGNAL(clicked()), this, SLOT(downClicked()));
            innerLayout->addWidget(down, row, col);
        }
        ++col;

        QPushButton *del = new QPushButton(m_innerFrame);
        del->setObjectName(QString("%1").arg(i));
        del->setIcon(IconLoader().load("datadelete"));
        del->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
        connect(del, SIGNAL(clicked()), this, SLOT(deleteClicked()));
        innerLayout->addWidget(del, row, col);
        ++col;

        QLabel *dirLabel = new QLabel(m_innerFrame);
        dirLabel->setObjectName(QString("%1").arg(i));
        dirLabel->setText(dir);
        innerLayout->addWidget(dirLabel, row, col);
        innerLayout->setColumnStretch(col, 10);
        ++col;
    }
}

void
PluginPathConfigurator::upClicked()
{
    bool ok = false;
    int n = sender()->objectName().toInt(&ok);
    if (!ok) {
        SVCERR << "upClicked: unable to find index" << endl;
        return;
    }
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i + 1 == n) {
            newPath.push_back(m_path[i+1]);
            newPath.push_back(m_path[i]);
            ++i;
        } else {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    populate();
}

void
PluginPathConfigurator::downClicked()
{
    bool ok = false;
    int n = sender()->objectName().toInt(&ok);
    if (!ok) {
        SVCERR << "downClicked: unable to find index" << endl;
        return;
    }
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i == n) {
            newPath.push_back(m_path[i+1]);
            newPath.push_back(m_path[i]);
            ++i;
        } else {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    populate();
}

void
PluginPathConfigurator::deleteClicked()
{
    bool ok = false;
    int n = sender()->objectName().toInt(&ok);
    if (!ok) {
        SVCERR << "deleteClicked: unable to find index" << endl;
        return;
    }
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i != n) {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    populate();
}
