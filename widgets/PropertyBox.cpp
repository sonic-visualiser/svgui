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

#include "PropertyBox.h"
#include "PluginParameterDialog.h"

#include "base/PropertyContainer.h"
#include "base/PlayParameters.h"
#include "layer/Layer.h"
#include "base/UnitDatabase.h"

#include "plugin/RealTimePluginFactory.h"
#include "plugin/RealTimePluginInstance.h"
#include "plugin/PluginXml.h"

#include "AudioDial.h"
#include "LEDButton.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

#include <cassert>
#include <iostream>
#include <cmath>

//#define DEBUG_PROPERTY_BOX 1

PropertyBox::PropertyBox(PropertyContainer *container) :
    m_container(container)
{
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "(\"" <<
	container->getPropertyContainerName().toStdString() << "\")]::PropertyBox" << std::endl;
#endif

    m_mainBox = new QVBoxLayout;
    setLayout(m_mainBox);

//    m_nameWidget = new QLabel;
//    m_mainBox->addWidget(m_nameWidget);
//    m_nameWidget->setText(container->objectName());

    m_mainWidget = new QWidget;
    m_mainBox->addWidget(m_mainWidget);
    m_mainBox->insertStretch(2, 10);

    m_viewPlayFrame = 0;
    populateViewPlayFrame();

    m_layout = new QGridLayout;
    m_layout->setMargin(0);
    m_mainWidget->setLayout(m_layout);

    PropertyContainer::PropertyList properties = m_container->getProperties();

    blockSignals(true);

    size_t i;

    for (i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i]);
    }

    blockSignals(false);

    m_layout->setRowStretch(m_layout->rowCount(), 10);

    connect(UnitDatabase::getInstance(), SIGNAL(unitDatabaseChanged()),
            this, SLOT(unitDatabaseChanged()));

#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "]::PropertyBox returning" << std::endl;
#endif
}

PropertyBox::~PropertyBox()
{
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "]::~PropertyBox" << std::endl;
#endif
}

void
PropertyBox::populateViewPlayFrame()
{
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox(" << m_container << ")::populateViewPlayFrame" << std::endl;
#endif

    if (m_viewPlayFrame) {
	delete m_viewPlayFrame;
	m_viewPlayFrame = 0;
    }

    if (!m_container) return;

    Layer *layer = dynamic_cast<Layer *>(m_container);
    if (layer) {
	connect(layer, SIGNAL(modelReplaced()),
		this, SLOT(populateViewPlayFrame()));
    }

    PlayParameters *params = m_container->getPlayParameters();
    if (!params && !layer) return;

    m_viewPlayFrame = new QFrame;
    m_viewPlayFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_mainBox->addWidget(m_viewPlayFrame);

    QHBoxLayout *layout = new QHBoxLayout;
    m_viewPlayFrame->setLayout(layout);

    layout->setMargin(layout->margin() / 2);

#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox::populateViewPlayFrame: container " << m_container << " (name " << m_container->getPropertyContainerName().toStdString() << ") params " << params << std::endl;
#endif

    if (layer) {
	QLabel *showLabel = new QLabel(tr("Show"));
	layout->addWidget(showLabel);
	layout->setAlignment(showLabel, Qt::AlignVCenter);

	LEDButton *showButton = new LEDButton(Qt::blue);
	layout->addWidget(showButton);
	connect(showButton, SIGNAL(stateChanged(bool)),
		this, SIGNAL(showLayer(bool)));
	layout->setAlignment(showButton, Qt::AlignVCenter);
    }
    
    if (params) {

	QLabel *playLabel = new QLabel(tr("Play"));
	layout->addWidget(playLabel);
	layout->setAlignment(playLabel, Qt::AlignVCenter);

	LEDButton *playButton = new LEDButton(Qt::darkGreen);
	layout->addWidget(playButton);
	connect(playButton, SIGNAL(stateChanged(bool)),
		params, SLOT(setPlayAudible(bool)));
	connect(params, SIGNAL(playAudibleChanged(bool)),
		playButton, SLOT(setState(bool)));
	layout->setAlignment(playButton, Qt::AlignVCenter);

	layout->insertStretch(-1, 10);

        if (params->getPlayPluginId() != "") {
            QPushButton *pluginButton = new QPushButton(QIcon(":icons/faders.png"), "");
            pluginButton->setFixedWidth(24);
            pluginButton->setFixedHeight(24);
            layout->addWidget(pluginButton);
            connect(pluginButton, SIGNAL(clicked()),
                    this, SLOT(editPlugin()));
        }

	AudioDial *gainDial = new AudioDial;
	layout->addWidget(gainDial);
	gainDial->setMeterColor(Qt::darkRed);
	gainDial->setMinimum(-50);
	gainDial->setMaximum(50);
	gainDial->setPageStep(1);
	gainDial->setFixedWidth(24);
	gainDial->setFixedHeight(24);
	gainDial->setNotchesVisible(false);
	gainDial->setToolTip(tr("Playback Level"));
	gainDial->setDefaultValue(0);
	connect(gainDial, SIGNAL(valueChanged(int)),
		this, SLOT(playGainDialChanged(int)));
	connect(params, SIGNAL(playGainChanged(float)),
		this, SLOT(playGainChanged(float)));
	connect(this, SIGNAL(changePlayGain(float)),
		params, SLOT(setPlayGain(float)));
	connect(this, SIGNAL(changePlayGainDial(int)),
		gainDial, SLOT(setValue(int)));
	layout->setAlignment(gainDial, Qt::AlignVCenter);

	AudioDial *panDial = new AudioDial;
	layout->addWidget(panDial);
	panDial->setMeterColor(Qt::darkGreen);
	panDial->setMinimum(-50);
	panDial->setMaximum(50);
	panDial->setPageStep(1);
	panDial->setFixedWidth(24);
	panDial->setFixedHeight(24);
	panDial->setNotchesVisible(false);
	panDial->setToolTip(tr("Playback Pan / Balance"));
	panDial->setDefaultValue(0);
	connect(panDial, SIGNAL(valueChanged(int)),
		this, SLOT(playPanDialChanged(int)));
	connect(params, SIGNAL(playPanChanged(float)),
		this, SLOT(playPanChanged(float)));
	connect(this, SIGNAL(changePlayPan(float)),
		params, SLOT(setPlayPan(float)));
	connect(this, SIGNAL(changePlayPanDial(int)),
		panDial, SLOT(setValue(int)));
	layout->setAlignment(panDial, Qt::AlignVCenter);

    } else {

	layout->insertStretch(-1, 10);
    }
}

void
PropertyBox::updatePropertyEditor(PropertyContainer::PropertyName name)
{
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);
    int row = m_layout->rowCount();

    int min = 0, max = 0, value = 0;
    value = m_container->getPropertyRangeAndValue(name, &min, &max);

    bool have = (m_propertyControllers.find(name) !=
		 m_propertyControllers.end());

    QString groupName = m_container->getPropertyGroupName(name);
    QString propertyLabel = m_container->getPropertyLabel(name);

#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this
	      << "(\"" << m_container->getPropertyContainerName().toStdString()
	      << "\")]";
    std::cerr << "::updatePropertyEditor(\"" << name.toStdString() << "\"):";
    std::cerr << " value " << value << ", have " << have << ", group \""
	      << groupName.toStdString() << "\"" << std::endl;
#endif

    bool inGroup = (groupName != QString());

    if (!have) {
	if (inGroup) {
	    if (m_groupLayouts.find(groupName) == m_groupLayouts.end()) {
#ifdef DEBUG_PROPERTY_BOX
		std::cerr << "PropertyBox: adding label \"" << groupName.toStdString() << "\" and frame for group for \"" << name.toStdString() << "\"" << std::endl;
#endif
		m_layout->addWidget(new QLabel(groupName, m_mainWidget), row, 0);
		QFrame *frame = new QFrame(m_mainWidget);
		m_layout->addWidget(frame, row, 1, 1, 2);
		m_groupLayouts[groupName] = new QHBoxLayout;
		m_groupLayouts[groupName]->setMargin(0);
		frame->setLayout(m_groupLayouts[groupName]);
	    }
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: adding label \"" << propertyLabel.toStdString() << "\"" << std::endl;
#endif
	    m_layout->addWidget(new QLabel(propertyLabel, m_mainWidget), row, 0);
	}
    }

    switch (type) {

    case PropertyContainer::ToggleProperty:
    {
	QCheckBox *cb;

	if (have) {
	    cb = dynamic_cast<QCheckBox *>(m_propertyControllers[name]);
	    assert(cb);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: creating new checkbox" << std::endl;
#endif
	    cb = new QCheckBox();
	    cb->setObjectName(name);
	    connect(cb, SIGNAL(stateChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));
	    if (inGroup) {
		cb->setToolTip(propertyLabel);
		m_groupLayouts[groupName]->addWidget(cb);
	    } else {
		m_layout->addWidget(cb, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = cb;
	}

	if (cb->isChecked() != (value > 0)) {
	    cb->blockSignals(true);
	    cb->setChecked(value > 0);
	    cb->blockSignals(false);
	}
	break;
    }

    case PropertyContainer::RangeProperty:
    {
	AudioDial *dial;

	if (have) {
	    dial = dynamic_cast<AudioDial *>(m_propertyControllers[name]);
	    assert(dial);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: creating new dial" << std::endl;
#endif
	    dial = new AudioDial();
	    dial->setObjectName(name);
	    dial->setMinimum(min);
	    dial->setMaximum(max);
	    dial->setPageStep(1);
	    dial->setNotchesVisible((max - min) <= 12);
	    dial->setDefaultValue(value);
	    connect(dial, SIGNAL(valueChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));

	    if (inGroup) {
		dial->setFixedWidth(24);
		dial->setFixedHeight(24);
		dial->setToolTip(propertyLabel);
		m_groupLayouts[groupName]->addWidget(dial);
	    } else {
		dial->setFixedWidth(32);
		dial->setFixedHeight(32);
		m_layout->addWidget(dial, row, 1);
		QLabel *label = new QLabel(m_mainWidget);
		connect(dial, SIGNAL(valueChanged(int)),
			label, SLOT(setNum(int)));
		label->setNum(value);
		m_layout->addWidget(label, row, 2);
	    }

	    m_propertyControllers[name] = dial;
	}

	if (dial->value() != value) {
	    dial->blockSignals(true);
	    dial->setValue(value);
	    dial->blockSignals(false);
	}
	break;
    }

    case PropertyContainer::ValueProperty:
    case PropertyContainer::UnitsProperty:
    {
	QComboBox *cb;

	if (have) {
	    cb = dynamic_cast<QComboBox *>(m_propertyControllers[name]);
	    assert(cb);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: creating new combobox" << std::endl;
#endif

	    cb = new QComboBox();
	    cb->setObjectName(name);
            cb->setDuplicatesEnabled(false);

            if (type == PropertyContainer::ValueProperty) {
                for (int i = min; i <= max; ++i) {
                    cb->addItem(m_container->getPropertyValueLabel(name, i));
                }
                cb->setEditable(false);
            } else {
                QStringList units = UnitDatabase::getInstance()->getKnownUnits();
                for (int i = 0; i < units.size(); ++i) {
                    cb->addItem(units[i]);
                }
                cb->setEditable(true);
            }

	    connect(cb, SIGNAL(activated(int)),
		    this, SLOT(propertyControllerChanged(int)));

	    if (inGroup) {
		cb->setToolTip(propertyLabel);
		m_groupLayouts[groupName]->addWidget(cb);
	    } else {
		m_layout->addWidget(cb, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = cb;
	}

        cb->blockSignals(true);
        if (type == PropertyContainer::ValueProperty) {
            if (cb->currentIndex() != value) {
                cb->setCurrentIndex(value);
            }
        } else {
            QString unit = UnitDatabase::getInstance()->getUnitById(value);
            if (cb->currentText() != unit) {
                for (int i = 0; i < cb->count(); ++i) {
                    if (cb->itemText(i) == unit) {
                        cb->setCurrentIndex(i);
                        break;
                    }
                }
            }
        }
        cb->blockSignals(false);

#ifdef Q_WS_MAC
	// Crashes on startup without this, for some reason
	cb->setMinimumSize(QSize(10, 10));
#endif

	break;
    }

    default:
	break;
    }
}

void
PropertyBox::propertyContainerPropertyChanged(PropertyContainer *pc)
{
    if (pc != m_container) return;
    
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox::propertyContainerPropertyChanged" << std::endl;
#endif

    PropertyContainer::PropertyList properties = m_container->getProperties();
    size_t i;

    blockSignals(true);

    for (i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i]);
    }

    blockSignals(false);
}

void
PropertyBox::unitDatabaseChanged()
{
    blockSignals(true);

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i]);
    }

    blockSignals(false);
}    

void
PropertyBox::propertyControllerChanged(int value)
{
    QObject *obj = sender();
    QString name = obj->objectName();

#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox::propertyControllerChanged(" << name.toStdString()
	      << ", " << value << ")" << std::endl;
#endif
    
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);

    if (type == PropertyContainer::UnitsProperty) {
        QComboBox *cb = dynamic_cast<QComboBox *>(obj);
        if (cb) {
            QString unit = cb->currentText();
            m_container->setPropertyWithCommand
                (name, UnitDatabase::getInstance()->getUnitId(unit));
        }
    } else if (type != PropertyContainer::InvalidProperty) {
	m_container->setPropertyWithCommand(name, value);
    }

    if (type == PropertyContainer::RangeProperty) {
	AudioDial *dial = dynamic_cast<AudioDial *>(m_propertyControllers[name]);
	if (dial) {
	    dial->setToolTip(QString("%1: %2").arg(name).arg(value));
	    //!!! unfortunately this doesn't update an already-visible tooltip
	}
    }
}
    
void
PropertyBox::playGainChanged(float gain)
{
    int dialValue = lrint(log10(gain) * 20.0);
    if (dialValue < -50) dialValue = -50;
    if (dialValue >  50) dialValue =  50;
    emit changePlayGainDial(dialValue);
}

void
PropertyBox::playGainDialChanged(int dialValue)
{
    float gain = pow(10, float(dialValue) / 20.0);
    emit changePlayGain(gain);
}
    
void
PropertyBox::playPanChanged(float pan)
{
    int dialValue = lrint(pan * 50.0);
    if (dialValue < -50) dialValue = -50;
    if (dialValue >  50) dialValue =  50;
    emit changePlayPanDial(dialValue);
}

void
PropertyBox::playPanDialChanged(int dialValue)
{
    float pan = float(dialValue) / 50.0;
    if (pan < -1.0) pan = -1.0;
    if (pan >  1.0) pan =  1.0;
    emit changePlayPan(pan);
}

void
PropertyBox::editPlugin()
{
    //!!! should probably just emit and let something else do this

    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    QString pluginId = params->getPlayPluginId();
    QString configurationXml = params->getPlayPluginConfiguration();
    
    RealTimePluginFactory *factory =
	RealTimePluginFactory::instanceFor(pluginId);
    if (!factory) return;

    RealTimePluginInstance *instance =
        factory->instantiatePlugin(pluginId, 0, 0, 48000, 1024, 1);
    if (!instance) return;

    PluginXml(instance).setParametersFromXml(configurationXml);

    PluginParameterDialog *dialog = new PluginParameterDialog(instance);
    connect(dialog, SIGNAL(pluginConfigurationChanged(QString)),
            this, SLOT(pluginConfigurationChanged(QString)));

    if (dialog->exec() == QDialog::Accepted) {
        params->setPlayPluginConfiguration(PluginXml(instance).toXmlString());
    } else {
        // restore in case we mucked about with the configuration
        // as a consequence of signals from the dialog
        params->setPlayPluginConfiguration(configurationXml);
    }

    delete dialog;
    delete instance;
}

void
PropertyBox::pluginConfigurationChanged(QString configurationXml)
{
    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    params->setPlayPluginConfiguration(configurationXml);
}    
    
    

#ifdef INCLUDE_MOCFILES
#include "PropertyBox.moc.cpp"
#endif

