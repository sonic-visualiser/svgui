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
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>

#include "IconLoader.h"
#include "WidgetScale.h"

PluginPathConfigurator::PluginPathConfigurator(QWidget *parent) :
    QFrame(parent)
{
    m_layout = new QGridLayout;
    setLayout(m_layout);

    int row = 0;
    
    m_header = new QLabel;
    m_header->setText(tr("Plugin locations"));
    m_layout->addWidget(m_header, row, 0);

    m_pluginTypeSelector = new QComboBox;
    m_layout->addWidget(m_pluginTypeSelector, row, 1);
    connect(m_pluginTypeSelector, SIGNAL(currentTextChanged(QString)),
            this, SLOT(currentTypeChanged(QString)));

    ++row;
    
    m_list = new QListWidget;
    m_layout->addWidget(m_list, row, 0, 1, 2);
    m_layout->setRowStretch(row, 10);
    m_layout->setColumnStretch(0, 10);
    connect(m_list, SIGNAL(currentRowChanged(int)),
            this, SLOT(currentLocationChanged(int)));
    ++row;

    QHBoxLayout *buttons = new QHBoxLayout;
    
    m_down = new QPushButton;
    m_down->setIcon(IconLoader().load("down"));
    m_down->setToolTip(tr("Move the selected location later in the list"));
    m_down->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(m_down, SIGNAL(clicked()), this, SLOT(downClicked()));
    buttons->addWidget(m_down);

    m_up = new QPushButton;
    m_up->setIcon(IconLoader().load("up"));
    m_up->setToolTip(tr("Move the selected location earlier in the list"));
    m_up->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(m_up, SIGNAL(clicked()), this, SLOT(upClicked()));
    buttons->addWidget(m_up);

    m_delete = new QPushButton;
    m_delete->setIcon(IconLoader().load("datadelete"));
    m_delete->setToolTip(tr("Remove the selected location from the list"));
    m_delete->setFixedSize(WidgetScale::scaleQSize(QSize(16, 16)));
    connect(m_delete, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    buttons->addWidget(m_delete);

    m_layout->addLayout(buttons, row, 1);
    ++row;
}

PluginPathConfigurator::~PluginPathConfigurator()
{
}

void
PluginPathConfigurator::setPaths(Paths paths)
{
    m_paths = paths;

    m_pluginTypeSelector->clear();
    for (const auto &p: paths) {
        m_pluginTypeSelector->addItem(p.first);
    }
    
    populate();
}

void
PluginPathConfigurator::populate()
{
    m_list->clear();

    if (m_paths.empty()) return;

    populateFor(m_paths.begin()->first, 0);
}

void
PluginPathConfigurator::populateFor(QString type, int makeCurrent)
{
    m_list->clear();

    for (int i = 0; i < m_pluginTypeSelector->count(); ++i) {
        if (type == m_pluginTypeSelector->itemText(i)) {
            m_pluginTypeSelector->setCurrentIndex(i);
        }
    }
    
    QStringList path = m_paths.at(type).directories;
    
    for (int i = 0; i < path.size(); ++i) {
        m_list->addItem(path[i]);
    }

    if (makeCurrent >= 0 && makeCurrent < path.size()) {
        m_list->setCurrentRow(makeCurrent);
    }
}

void
PluginPathConfigurator::currentLocationChanged(int i)
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    m_up->setEnabled(i > 0);
    m_down->setEnabled(i + 1 < path.size());
    m_delete->setEnabled(i < path.size());
}

void
PluginPathConfigurator::currentTypeChanged(QString type)
{
    populateFor(type, 0);
}

void
PluginPathConfigurator::upClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    QString variable = m_paths.at(type).envVariable;
        
    int current = m_list->currentRow();
    if (current <= 0) return;
    
    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i + 1 == current) {
            newPath.push_back(path[i+1]);
            newPath.push_back(path[i]);
            ++i;
        } else {
            newPath.push_back(path[i]);
        }
    }
    m_paths[type] = { newPath, variable };
    
    populateFor(type, current - 1);
}

void
PluginPathConfigurator::downClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    QString variable = m_paths.at(type).envVariable;

    int current = m_list->currentRow();
    if (current < 0 || current + 1 >= path.size()) return;

    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i == current) {
            newPath.push_back(path[i+1]);
            newPath.push_back(path[i]);
            ++i;
        } else {
            newPath.push_back(path[i]);
        }
    }
    m_paths[type] = { newPath, variable };
    
    populateFor(type, current + 1);
}

void
PluginPathConfigurator::deleteClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    QString variable = m_paths.at(type).envVariable;
    
    int current = m_list->currentRow();

    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i != current) {
            newPath.push_back(path[i]);
        }
    }
    m_paths[type] = { newPath, variable };
    
    populateFor(type, current < newPath.size() ? current : current-1);
}
