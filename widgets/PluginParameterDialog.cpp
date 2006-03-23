/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PluginParameterDialog.h"

#include "PluginParameterBox.h"

#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>

PluginParameterDialog::PluginParameterDialog(PluginInstance *plugin,
					     QWidget *parent) :
    QDialog(parent),
    m_plugin(plugin),
    m_parameterBox(0)
{
    QGridLayout *grid = new QGridLayout;
    setLayout(grid);

    QGroupBox *pluginBox = new QGroupBox;
    pluginBox->setTitle(tr("Plugin"));
    grid->addWidget(pluginBox, 0, 0);

    QGridLayout *subgrid = new QGridLayout;
    pluginBox->setLayout(subgrid);

    subgrid->setSpacing(0);
    subgrid->setMargin(10);

    QFont font(pluginBox->font());
    font.setBold(true);

    QLabel *nameLabel = new QLabel(plugin->getDescription().c_str());
    nameLabel->setFont(font);

    QLabel *makerLabel = new QLabel(plugin->getMaker().c_str());
    makerLabel->setFont(font);

    QLabel *versionLabel = new QLabel(QString("%1")
                                      .arg(plugin->getPluginVersion()));
    versionLabel->setFont(font);

    QLabel *copyrightLabel = new QLabel(plugin->getCopyright().c_str());
    copyrightLabel->setFont(font);

    QLabel *typeLabel = new QLabel(plugin->getType().c_str());
    typeLabel->setFont(font);

    subgrid->addWidget(new QLabel(tr("Name:")), 0, 0);
    subgrid->addWidget(nameLabel, 0, 1);

    subgrid->addWidget(new QLabel(tr("Type:")), 1, 0);
    subgrid->addWidget(typeLabel, 1, 1);

    subgrid->addWidget(new QLabel(tr("Maker:")), 2, 0);
    subgrid->addWidget(makerLabel, 2, 1);

    subgrid->addWidget(new QLabel(tr("Copyright:  ")), 3, 0);
    subgrid->addWidget(copyrightLabel, 3, 1);

    subgrid->addWidget(new QLabel(tr("Version:")), 4, 0);
    subgrid->addWidget(versionLabel, 4, 1);

    subgrid->setColumnStretch(1, 2);

    QGroupBox *paramBox = new QGroupBox;
    paramBox->setTitle(tr("Plugin Parameters"));
    grid->addWidget(paramBox, 1, 0);
    grid->setRowStretch(1, 10);

    QHBoxLayout *paramLayout = new QHBoxLayout;
    paramLayout->setMargin(0);
    paramBox->setLayout(paramLayout);

    m_parameterBox = new PluginParameterBox(m_plugin);
    paramLayout->addWidget(m_parameterBox);

    QHBoxLayout *hbox = new QHBoxLayout;
    grid->addLayout(hbox, 2, 0);
    
    QPushButton *ok = new QPushButton(tr("OK"));
    QPushButton *cancel = new QPushButton(tr("Cancel"));
    hbox->addStretch(10);
    hbox->addWidget(ok);
    hbox->addWidget(cancel);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
}

PluginParameterDialog::~PluginParameterDialog()
{
}

