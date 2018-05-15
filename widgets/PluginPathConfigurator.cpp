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
#include <QListWidget>
#include <QLabel>

#include "IconLoader.h"
#include "WidgetScale.h"

PluginPathConfigurator::PluginPathConfigurator(QWidget *parent) :
    QFrame(parent)
{
    m_layout = new QGridLayout;
    setLayout(m_layout);

    int row = 0;
    
    QLabel *header = new QLabel;
    header->setText(tr("Plugin locations"));
    m_layout->addWidget(header, row, 0);
    ++row;
    
    m_list = new QListWidget;
    m_layout->addWidget(m_list, row, 0, 1, 2);
    m_layout->setRowStretch(row, 10);
    m_layout->setColumnStretch(0, 10);
    ++row;

    QHBoxLayout *buttons = new QHBoxLayout;
    
    QPushButton *down = new QPushButton;
    down->setIcon(IconLoader().load("down"));
    down->setToolTip(tr("Move the selected location down in the list"));
    down->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(down, SIGNAL(clicked()), this, SLOT(downClicked()));
    buttons->addWidget(down);

    QPushButton *up = new QPushButton;
    up->setIcon(IconLoader().load("up"));
    up->setToolTip(tr("Move the selected location up in the list"));
    up->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(up, SIGNAL(clicked()), this, SLOT(upClicked()));
    buttons->addWidget(up);

    QPushButton *del = new QPushButton;
    del->setIcon(IconLoader().load("datadelete"));
    del->setToolTip(tr("Remove the selected location from the list"));
    del->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(del, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    buttons->addWidget(del);

    m_layout->addLayout(buttons, row, 1);
    ++row;
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
PluginPathConfigurator::populate(int makeCurrent)
{
    m_list->clear();

    for (int i = 0; i < m_path.size(); ++i) {
        m_list->addItem(m_path[i]);
    }

    if (makeCurrent >= 0 && makeCurrent < m_path.size()) {
        m_list->setCurrentRow(makeCurrent);
    }
}

void
PluginPathConfigurator::upClicked()
{
    int current = m_list->currentRow();
    if (current <= 0) return;
    
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i + 1 == current) {
            newPath.push_back(m_path[i+1]);
            newPath.push_back(m_path[i]);
            ++i;
        } else {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    
    populate(current - 1);
}

void
PluginPathConfigurator::downClicked()
{
    int current = m_list->currentRow();
    if (current < 0 || current + 1 >= m_path.size()) return;
    
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i == current) {
            newPath.push_back(m_path[i+1]);
            newPath.push_back(m_path[i]);
            ++i;
        } else {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    
    populate(current + 1);
}

void
PluginPathConfigurator::deleteClicked()
{
    int current = m_list->currentRow();
    
    QStringList newPath;
    for (int i = 0; i < m_path.size(); ++i) {
        if (i != current) {
            newPath.push_back(m_path[i]);
        }
    }
    m_path = newPath;
    
    populate(current < m_path.size() ? current : current-1);
}
