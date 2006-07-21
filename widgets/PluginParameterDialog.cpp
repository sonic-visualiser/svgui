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

#include "vamp-sdk/Plugin.h"

#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>

PluginParameterDialog::PluginParameterDialog(Vamp::PluginBase *plugin,
                                             int sourceChannels,
                                             int targetChannels,
                                             int defaultChannel,
                                             QString output,
					     QWidget *parent) :
    QDialog(parent),
    m_plugin(plugin),
    m_channel(defaultChannel),
    m_parameterBox(0)
{
    setWindowTitle(tr("Plugin Parameters"));

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

    QLabel *outputLabel = 0;

    if (output != "") {

        Vamp::Plugin *fePlugin = dynamic_cast<Vamp::Plugin *>(plugin);

        if (fePlugin) {

            std::vector<Vamp::Plugin::OutputDescriptor> od =
                fePlugin->getOutputDescriptors();

            if (od.size() > 1) {

                for (size_t i = 0; i < od.size(); ++i) {
                    if (od[i].name == output.toStdString()) {
                        outputLabel = new QLabel(od[i].description.c_str());
                        break;
                    }
                }
            }
        }
    }

    QLabel *versionLabel = new QLabel(QString("%1")
                                      .arg(plugin->getPluginVersion()));

    QLabel *copyrightLabel = new QLabel(plugin->getCopyright().c_str());

    QLabel *typeLabel = new QLabel(plugin->getType().c_str());
    typeLabel->setFont(font);

    subgrid->addWidget(new QLabel(tr("Name:")), 0, 0);
    subgrid->addWidget(nameLabel, 0, 1);

    subgrid->addWidget(new QLabel(tr("Type:")), 1, 0);
    subgrid->addWidget(typeLabel, 1, 1);

    int outputOffset = 0;
    if (outputLabel) {
        subgrid->addWidget(new QLabel(tr("Output:")), 2, 0);
        subgrid->addWidget(outputLabel, 2, 1);
        outputOffset = 1;
    }

    subgrid->addWidget(new QLabel(tr("Maker:")), 2 + outputOffset, 0);
    subgrid->addWidget(makerLabel, 2 + outputOffset, 1);

    subgrid->addWidget(new QLabel(tr("Copyright:  ")), 3 + outputOffset, 0);
    subgrid->addWidget(copyrightLabel, 3 + outputOffset, 1);

    subgrid->addWidget(new QLabel(tr("Version:")), 4 + outputOffset, 0);
    subgrid->addWidget(versionLabel, 4 + outputOffset, 1);

    subgrid->setColumnStretch(1, 2);

    QGroupBox *paramBox = new QGroupBox;
    paramBox->setTitle(tr("Plugin Parameters"));
    grid->addWidget(paramBox, 1, 0);
    grid->setRowStretch(1, 10);

    QHBoxLayout *paramLayout = new QHBoxLayout;
    paramLayout->setMargin(0);
    paramBox->setLayout(paramLayout);

    m_parameterBox = new PluginParameterBox(m_plugin);
    connect(m_parameterBox, SIGNAL(pluginConfigurationChanged(QString)),
            this,  SIGNAL(pluginConfigurationChanged(QString)));
    paramLayout->addWidget(m_parameterBox);

    if (sourceChannels != targetChannels) {

        // At the moment we can only cope with the case where
        // sourceChannels > targetChannels and targetChannels == 1

        if (sourceChannels < targetChannels) {

            QMessageBox::warning
                (parent,
                 tr("Channel mismatch"),
                 tr("This plugin requires at least %1 input channels, but only %2 %3 available.  The plugin probably will not work correctly.").arg(targetChannels).arg(sourceChannels).arg(sourceChannels != 1 ? tr("are") : tr("is")),
                 QMessageBox::Ok,
                 QMessageBox::NoButton);

        } else {

            QGroupBox *channelBox = new QGroupBox;
            channelBox->setTitle(tr("Channels"));
            grid->addWidget(channelBox, 2, 0);
            
            QVBoxLayout *channelLayout = new QVBoxLayout;
            channelBox->setLayout(channelLayout);

            if (targetChannels != 1) {

                channelLayout->addWidget
                    (new QLabel(tr("This plugin accepts no more than %1 input channels,\nbut %2 are available.  Only the first %3 will be used.\n")
                                .arg(targetChannels)
                                .arg(sourceChannels)
                                .arg(targetChannels)));

            } else {

                channelLayout->addWidget(new QLabel(tr("This plugin only has a single channel input,\nbut the source has %1 channels.").arg(sourceChannels)));

                QComboBox *channelCombo = new QComboBox;
                channelCombo->addItem(tr("Use mean of source channels"));
                for (int i = 0; i < sourceChannels; ++i) {
                    channelCombo->addItem(tr("Use channel %1 only").arg(i + 1));
                }

                connect(channelCombo, SIGNAL(activated(int)),
                        this, SLOT(channelComboChanged(int)));

                channelLayout->addWidget(channelCombo);
            }
        }
    }

    QHBoxLayout *hbox = new QHBoxLayout;
    grid->addLayout(hbox, 3, 0);
    
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

void
PluginParameterDialog::channelComboChanged(int index)
{
    m_channel = index - 1;
}

