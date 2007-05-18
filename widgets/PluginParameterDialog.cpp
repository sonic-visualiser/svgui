/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PluginParameterDialog.h"

#include "PluginParameterBox.h"
#include "WindowTypeSelector.h"

#include "vamp-sdk/Plugin.h"
#include "vamp-sdk/PluginHostAdapter.h"

#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QMessageBox>
#include <QComboBox>
#include <QSettings>

PluginParameterDialog::PluginParameterDialog(Vamp::PluginBase *plugin,
					     QWidget *parent) :
    QDialog(parent),
    m_plugin(plugin),
    m_channel(-1),
    m_stepSize(0),
    m_blockSize(0),
    m_windowType(HanningWindow),
    m_parameterBox(0)
{
    setWindowTitle(tr("Plugin Parameters"));

    QGridLayout *grid = new QGridLayout;
    setLayout(grid);

    QGroupBox *pluginBox = new QGroupBox;
    pluginBox->setTitle(plugin->getType().c_str());
    grid->addWidget(pluginBox, 0, 0);

    QGridLayout *subgrid = new QGridLayout;
    pluginBox->setLayout(subgrid);

    subgrid->setSpacing(0);
    subgrid->setMargin(10);

    QFont boldFont(pluginBox->font());
    boldFont.setBold(true);

    QFont italicFont(pluginBox->font());
    italicFont.setItalic(true);

    QLabel *nameLabel = new QLabel(plugin->getName().c_str());
    nameLabel->setWordWrap(true);
    nameLabel->setFont(boldFont);

    QLabel *makerLabel = new QLabel(plugin->getMaker().c_str());
    makerLabel->setWordWrap(true);

    QLabel *versionLabel = new QLabel(QString("%1")
                                      .arg(plugin->getPluginVersion()));
    versionLabel->setWordWrap(true);

    QLabel *copyrightLabel = new QLabel(plugin->getCopyright().c_str());
    copyrightLabel->setWordWrap(true);

//    QLabel *typeLabel = new QLabel(plugin->getType().c_str());
//    typeLabel->setWordWrap(true);
//    typeLabel->setFont(boldFont);

    QLabel *descriptionLabel = 0;
    if (plugin->getDescription() != "") {
        descriptionLabel = new QLabel(plugin->getDescription().c_str());
        descriptionLabel->setWordWrap(true);
        descriptionLabel->setFont(italicFont);
    }

    int row = 0;

    QLabel *label = new QLabel(tr("Name:"));
    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    subgrid->addWidget(label, row, 0);
    subgrid->addWidget(nameLabel, row, 1);
    row++;

    if (descriptionLabel) {
//        label = new QLabel(tr("Description:"));
//        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
//        subgrid->addWidget(label, row, 0);
        subgrid->addWidget(descriptionLabel, row, 1);
        row++;
    }

    Vamp::PluginHostAdapter *fePlugin =
        dynamic_cast<Vamp::PluginHostAdapter *>(m_plugin);

    if (fePlugin) {
        label = new QLabel(tr("Version:"));
        label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        subgrid->addWidget(label, row, 0);
        subgrid->addWidget(versionLabel, row, 1);
        row++;
    }

//    label = new QLabel(tr("Type:"));
//    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
//    subgrid->addWidget(label, row, 0);
//    subgrid->addWidget(typeLabel, row, 1);
//    row++;

    label = new QLabel(tr("Maker:"));
    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    subgrid->addWidget(label, row, 0);
    subgrid->addWidget(makerLabel, row, 1);
    row++;

    label = new QLabel(tr("Copyright:  "));
    label->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    subgrid->addWidget(label, row, 0);
    subgrid->addWidget(copyrightLabel, row, 1);
    row++;
    
    m_outputSpacer = new QLabel;
    subgrid->addWidget(m_outputSpacer, row, 0);
    m_outputSpacer->setFixedHeight(7);
    m_outputSpacer->hide();
    row++;

    m_outputLabel = new QLabel(tr("Output:"));
    m_outputLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    subgrid->addWidget(m_outputLabel, row, 0);
    m_outputValue = new QLabel;
    m_outputValue->setFont(boldFont);
    subgrid->addWidget(m_outputValue, row, 1);
    m_outputLabel->hide();
    m_outputValue->hide();
    row++;

    m_outputDescription = new QLabel;
    m_outputDescription->setFont(italicFont);
    subgrid->addWidget(m_outputDescription, row, 1);
    m_outputDescription->hide();
    row++;

    subgrid->setColumnStretch(1, 2);

    m_inputModelBox = new QGroupBox;
    m_inputModelBox->setTitle(tr("Input Source"));
    grid->addWidget(m_inputModelBox, 1, 0);
    
    m_inputModels = new QComboBox;
    QHBoxLayout *inputLayout = new QHBoxLayout;
    m_inputModelBox->setLayout(inputLayout);
    inputLayout->addWidget(m_inputModels);
    m_inputModelBox->hide();

    QGroupBox *paramBox = new QGroupBox;
    paramBox->setTitle(tr("Plugin Parameters"));
    grid->addWidget(paramBox, 2, 0);
    grid->setRowStretch(2, 10);

    QHBoxLayout *paramLayout = new QHBoxLayout;
    paramLayout->setMargin(0);
    paramBox->setLayout(paramLayout);

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    paramLayout->addWidget(scroll);

    m_parameterBox = new PluginParameterBox(m_plugin);
    connect(m_parameterBox, SIGNAL(pluginConfigurationChanged(QString)),
            this,  SIGNAL(pluginConfigurationChanged(QString)));
    scroll->setWidget(m_parameterBox);

    m_advanced = new QFrame;
    QVBoxLayout *advancedLayout = new QVBoxLayout;
    advancedLayout->setMargin(0);
    m_advanced->setLayout(advancedLayout);
    grid->addWidget(m_advanced, 3, 0);

    m_channelBox = new QGroupBox;
    m_channelBox->setTitle(tr("Channels"));
    advancedLayout->addWidget(m_channelBox);
    m_channelBox->setVisible(false);
    m_haveChannelBoxData = false;

    m_windowBox = new QGroupBox;
    m_windowBox->setTitle(tr("Processing"));
    advancedLayout->addWidget(m_windowBox);
    m_windowBox->setVisible(false);
    m_haveWindowBoxData = false;

    QHBoxLayout *hbox = new QHBoxLayout;
    grid->addLayout(hbox, 4, 0);

    m_advancedVisible = false;

    m_advancedButton = new QPushButton(tr("Advanced >>"));
    m_advancedButton->setCheckable(true);
    connect(m_advancedButton, SIGNAL(clicked()), this, SLOT(advancedToggled()));
        
    QSettings settings;
    settings.beginGroup("PluginParameterDialog");
    m_advancedVisible = settings.value("advancedvisible", false).toBool();
    settings.endGroup();
    
    m_advanced->setVisible(false);

    hbox->addWidget(m_advancedButton);
    m_advancedButton->hide();

    QPushButton *ok = new QPushButton(tr("OK"));
    QPushButton *cancel = new QPushButton(tr("Cancel"));
    ok->setDefault(true);
    hbox->addStretch(10);
    hbox->addWidget(ok);
    hbox->addWidget(cancel);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));

    setAdvancedVisible(m_advancedVisible);
}

PluginParameterDialog::~PluginParameterDialog()
{
}


void
PluginParameterDialog::setOutputLabel(QString text,
                                      QString description)
{
    if (text == "") {
        m_outputSpacer->hide();
        m_outputLabel->hide();
        m_outputValue->hide();
        m_outputDescription->hide();
    } else {
        m_outputSpacer->show();
        m_outputValue->setText(text);
        m_outputValue->setWordWrap(true);
        m_outputDescription->setText(description);
        m_outputLabel->show();
        m_outputValue->show();
        if (description != "") {
            m_outputDescription->show();
        } else {
            m_outputDescription->hide();
        }
    }
}

void
PluginParameterDialog::setChannelArrangement(int sourceChannels,
                                             int targetChannels,
                                             int defaultChannel)
{
    m_channel = defaultChannel;

    if (sourceChannels != targetChannels) {

        // At the moment we can only cope with the case where
        // sourceChannels > targetChannels and targetChannels == 1

        if (sourceChannels < targetChannels) {

            QMessageBox::warning
                (parentWidget(),
                 tr("Channel mismatch"),
                 tr("This plugin requires at least %1 input channels, but only %2 %3 available.  The plugin probably will not work correctly.").arg(targetChannels).arg(sourceChannels).arg(sourceChannels != 1 ? tr("are") : tr("is")),
                 QMessageBox::Ok,
                 QMessageBox::NoButton);

        } else {

            if (m_haveChannelBoxData) {
                std::cerr << "WARNING: PluginParameterDialog::setChannelArrangement: Calling more than once on same dialog is not currently implemented" << std::endl;
                return;
            }
            
            QVBoxLayout *channelLayout = new QVBoxLayout;
            m_channelBox->setLayout(channelLayout);

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

            m_channelBox->setVisible(true);
            m_haveChannelBoxData = true;
            m_advancedButton->show();
        }
    }

    setAdvancedVisible(m_advancedVisible);
}

void
PluginParameterDialog::setShowProcessingOptions(bool showWindowSize,
                                                bool showFrequencyDomainOptions)
{
    if (m_haveWindowBoxData) {
        std::cerr << "WARNING: PluginParameterDialog::setShowProcessingOptions: Calling more than once on same dialog is not currently implemented" << std::endl;
        return;
    }

    if (showWindowSize) {

        Vamp::PluginHostAdapter *fePlugin = dynamic_cast<Vamp::PluginHostAdapter *>(m_plugin);
        int size = 1024;
        int increment = 1024;
        if (fePlugin) {
            size = fePlugin->getPreferredBlockSize();
            std::cerr << "Feature extraction plugin \"" << fePlugin->getName() << "\" reports preferred block size as " << size << std::endl;
            if (size == 0) size = 1024;
            increment = fePlugin->getPreferredStepSize();
            if (increment == 0) {
                if (fePlugin->getInputDomain() == Vamp::Plugin::TimeDomain) {
                    increment = size;
                } else {
                    increment = size/2;
                }
            }
        }

        QGridLayout *windowLayout = new QGridLayout;
        m_windowBox->setLayout(windowLayout);

        if (showFrequencyDomainOptions) {
            windowLayout->addWidget(new QLabel(tr("Window size:")), 0, 0);
        } else {
            windowLayout->addWidget(new QLabel(tr("Audio frames per block:")), 0, 0);
        }

        std::cerr << "size: " << size << ", increment: " << increment << std::endl;

        QComboBox *blockSizeCombo = new QComboBox;
        blockSizeCombo->setEditable(true);
        bool found = false;
        for (int i = 0; i < 14; ++i) {
            int val = 1 << (i + 3);
            blockSizeCombo->addItem(QString("%1").arg(val));
            if (val == size) {
                blockSizeCombo->setCurrentIndex(i);
                found = true;
            }
        }
        if (!found) {
            blockSizeCombo->addItem(QString("%1").arg(size));
            blockSizeCombo->setCurrentIndex(blockSizeCombo->count() - 1);
        }
        blockSizeCombo->setValidator(new QIntValidator(1, pow(2, 18), this));
        connect(blockSizeCombo, SIGNAL(editTextChanged(const QString &)),
                this, SLOT(blockSizeComboChanged(const QString &)));
        windowLayout->addWidget(blockSizeCombo, 0, 1);

        windowLayout->addWidget(new QLabel(tr("Window increment:")), 1, 0);
        
        QComboBox *incrementCombo = new QComboBox;
        incrementCombo->setEditable(true);
        found = false;
        for (int i = 0; i < 14; ++i) {
            int val = 1 << (i + 3);
            incrementCombo->addItem(QString("%1").arg(val));
            if (val == increment) {
                incrementCombo->setCurrentIndex(i);
                found = true;
            }
        }
        if (!found) {
            incrementCombo->addItem(QString("%1").arg(increment));
            incrementCombo->setCurrentIndex(incrementCombo->count() - 1);
        }
        incrementCombo->setValidator(new QIntValidator(1, pow(2, 18), this));
        connect(incrementCombo, SIGNAL(editTextChanged(const QString &)),
                this, SLOT(incrementComboChanged(const QString &)));
        windowLayout->addWidget(incrementCombo, 1, 1);
        
        if (showFrequencyDomainOptions) {
            
            windowLayout->addWidget(new QLabel(tr("Window shape:")), 2, 0);
            WindowTypeSelector *windowTypeSelector = new WindowTypeSelector;
            connect(windowTypeSelector, SIGNAL(windowTypeChanged(WindowType)),
                    this, SLOT(windowTypeChanged(WindowType)));
            windowLayout->addWidget(windowTypeSelector, 2, 1);
        }

        m_windowBox->setVisible(true);
        m_haveWindowBoxData = true;
        m_advancedButton->show();
    }

    setAdvancedVisible(m_advancedVisible);
}

void
PluginParameterDialog::setCandidateInputModels(const QStringList &models)
{
    m_inputModels->clear();
    m_inputModels->insertItems(0, models);
    connect(m_inputModels, SIGNAL(activated(const QString &)),
            this, SIGNAL(inputModelChanged(QString)));
    m_inputModelBox->show();
}

QString
PluginParameterDialog::getInputModel() const
{
    return m_inputModels->currentText();
}

void
PluginParameterDialog::getProcessingParameters(size_t &blockSize) const
{
    blockSize = m_blockSize;
    return;
}

void
PluginParameterDialog::getProcessingParameters(size_t &stepSize,
                                               size_t &blockSize,
                                               WindowType &windowType) const
{
    stepSize = m_stepSize;
    blockSize = m_blockSize;
    windowType = m_windowType;
    return;
}

void
PluginParameterDialog::blockSizeComboChanged(const QString &text)
{
    m_blockSize = text.toInt();
    std::cerr << "Block size changed to " << m_blockSize << std::endl;
}

void
PluginParameterDialog::incrementComboChanged(const QString &text)
{
    m_stepSize = text.toInt();
    //!!! rename increment to step size throughout
    std::cerr << "Increment changed to " << m_stepSize << std::endl;
}

void
PluginParameterDialog::windowTypeChanged(WindowType type)
{
    m_windowType = type;
}

void
PluginParameterDialog::advancedToggled()
{
    setAdvancedVisible(!m_advancedVisible);
}

void
PluginParameterDialog::setAdvancedVisible(bool visible)
{
    m_advanced->setVisible(visible);

    if (visible) {
        m_advancedButton->setText(tr("Advanced <<"));
        m_advancedButton->setChecked(true);
    } else {
        m_advancedButton->setText(tr("Advanced >>"));
        m_advancedButton->setChecked(false);
    }

    QSettings settings;
    settings.beginGroup("PluginParameterDialog");
    settings.setValue("advancedvisible", visible);
    settings.endGroup();

//    std::cerr << "resize to " << sizeHint().width() << " x " << sizeHint().height() << std::endl;

    setMinimumHeight(sizeHint().height());
    adjustSize();

    m_advancedVisible = visible;

//    if (visible) setMaximumHeight(sizeHint().height());
//    adjustSize();
}

void
PluginParameterDialog::channelComboChanged(int index)
{
    m_channel = index - 1;
}

