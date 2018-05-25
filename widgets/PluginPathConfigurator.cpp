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
#include "PluginReviewDialog.h"

#include <QPushButton>
#include <QListWidget>
#include <QGridLayout>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QFileDialog>

#include "IconLoader.h"
#include "WidgetScale.h"

PluginPathConfigurator::PluginPathConfigurator(QWidget *parent) :
    QFrame(parent)
{
    m_layout = new QGridLayout;
    setLayout(m_layout);

    QHBoxLayout *buttons = new QHBoxLayout;

    m_down = new QPushButton;
    m_down->setIcon(IconLoader().load("down"));
    m_down->setToolTip(tr("Move the selected location later in the list"));
    connect(m_down, SIGNAL(clicked()), this, SLOT(downClicked()));
    buttons->addWidget(m_down);

    m_up = new QPushButton;
    m_up->setIcon(IconLoader().load("up"));
    m_up->setToolTip(tr("Move the selected location earlier in the list"));
    connect(m_up, SIGNAL(clicked()), this, SLOT(upClicked()));
    buttons->addWidget(m_up);

    m_add = new QPushButton;
    m_add->setIcon(IconLoader().load("plus"));
    m_add->setToolTip(tr("Move the selected location earlier in the list"));
    connect(m_add, SIGNAL(clicked()), this, SLOT(addClicked()));
    buttons->addWidget(m_add);
    
    m_delete = new QPushButton;
    m_delete->setIcon(IconLoader().load("datadelete"));
    m_delete->setToolTip(tr("Remove the selected location from the list"));
    connect(m_delete, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    buttons->addWidget(m_delete);

    m_reset = new QPushButton;
    m_reset->setText(tr("Reset"));
    m_reset->setToolTip(tr("Reset the list for this plugin type to its default"));
    connect(m_reset, SIGNAL(clicked()), this, SLOT(resetClicked()));
    buttons->addWidget(m_reset);

    int row = 0;
    
    m_header = new QLabel;
    m_header->setText(tr("Plugin locations for plugin type:"));
    m_layout->addWidget(m_header, row, 0);

    m_pluginTypeSelector = new QComboBox;
    m_layout->addWidget(m_pluginTypeSelector, row, 1, Qt::AlignLeft);
    connect(m_pluginTypeSelector, SIGNAL(currentTextChanged(QString)),
            this, SLOT(currentTypeChanged(QString)));

    m_layout->setColumnStretch(1, 10);
    ++row;
    
    m_list = new QListWidget;
    m_layout->addWidget(m_list, row, 0, 1, 3);
    m_layout->setRowStretch(row, 20);
    connect(m_list, SIGNAL(currentRowChanged(int)),
            this, SLOT(currentLocationChanged(int)));
    ++row;

    m_layout->addLayout(buttons, row, 0, Qt::AlignLeft);

    m_seePlugins = new QPushButton;
    m_seePlugins->setText(tr("Review plugins..."));
    connect(m_seePlugins, SIGNAL(clicked()), this, SLOT(seePluginsClicked()));
    m_layout->addWidget(m_seePlugins, row, 2);
    
    ++row;

    m_envOverride = new QCheckBox;
    connect(m_envOverride, SIGNAL(stateChanged(int)),
            this, SLOT(envOverrideChanged(int)));
    m_layout->addWidget(m_envOverride, row, 0, 1, 3);
    ++row;
}

PluginPathConfigurator::~PluginPathConfigurator()
{
}

void
PluginPathConfigurator::setPaths(Paths paths)
{
    m_paths = paths;
    if (m_originalPaths.empty()) {
        m_originalPaths = paths;
    }

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

    populateFor(m_paths.begin()->first, -1);
}

void
PluginPathConfigurator::populateFor(QString type, int makeCurrent)
{
    QString envVariable = m_paths.at(type).envVariable;
    bool useEnvVariable = m_paths.at(type).useEnvVariable;
    m_envOverride->setText
        (tr("Allow the %1 environment variable to take priority over this")
         .arg(envVariable));
    m_envOverride->setCheckState(useEnvVariable ? Qt::Checked : Qt::Unchecked);
    
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

    if (makeCurrent < path.size()) {
        m_list->setCurrentRow(makeCurrent);
        currentLocationChanged(makeCurrent);
    }
}

void
PluginPathConfigurator::currentLocationChanged(int i)
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    m_up->setEnabled(i > 0);
    m_down->setEnabled(i >= 0 && i + 1 < path.size());
    m_delete->setEnabled(i >= 0 && i < path.size());
    m_reset->setEnabled(path != m_originalPaths.at(type).directories);
}

void
PluginPathConfigurator::currentTypeChanged(QString type)
{
    populateFor(type, -1);
}

void
PluginPathConfigurator::envOverrideChanged(int state)
{
    bool useEnvVariable = (state == Qt::Checked);
    
    QString type = m_pluginTypeSelector->currentText();

    auto newEntry = m_paths.at(type);
    newEntry.useEnvVariable = useEnvVariable;
    m_paths[type] = newEntry;

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::upClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
        
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

    auto newEntry = m_paths.at(type);
    newEntry.directories = newPath;
    m_paths[type] = newEntry;
    
    populateFor(type, current - 1);

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::downClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;

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

    auto newEntry = m_paths.at(type);
    newEntry.directories = newPath;
    m_paths[type] = newEntry;
    
    populateFor(type, current + 1);

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::addClicked()
{
    QString type = m_pluginTypeSelector->currentText();

    QString newDir = QFileDialog::getExistingDirectory
        (this, tr("Choose directory to add"));

    if (newDir == QString()) return;

    auto newEntry = m_paths.at(type);
    newEntry.directories.push_back(newDir);
    m_paths[type] = newEntry;
    
    populateFor(type, newEntry.directories.size() - 1);

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::deleteClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    QStringList path = m_paths.at(type).directories;
    
    int current = m_list->currentRow();
    if (current < 0) return;

    QStringList newPath;
    for (int i = 0; i < path.size(); ++i) {
        if (i != current) {
            newPath.push_back(path[i]);
        }
    }

    auto newEntry = m_paths.at(type);
    newEntry.directories = newPath;
    m_paths[type] = newEntry;
    
    populateFor(type, current < newPath.size() ? current : current-1);

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::resetClicked()
{
    QString type = m_pluginTypeSelector->currentText();
    m_paths[type] = m_originalPaths[type];
    populateFor(type, -1);

    emit pathsChanged(m_paths);
}

void
PluginPathConfigurator::seePluginsClicked()
{
    PluginReviewDialog dialog(this);
    dialog.populate();
    dialog.exec();
}

