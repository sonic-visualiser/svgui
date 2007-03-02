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

#include "PropertyBox.h"
#include "PluginParameterDialog.h"

#include "base/PropertyContainer.h"
#include "base/PlayParameters.h"
#include "layer/Layer.h"
#include "base/UnitDatabase.h"
#include "base/RangeMapper.h"

#include "plugin/RealTimePluginFactory.h"
#include "plugin/RealTimePluginInstance.h"
#include "plugin/PluginXml.h"

#include "AudioDial.h"
#include "LEDButton.h"

#include "NotifyingCheckBox.h"
#include "NotifyingComboBox.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>

#include <cassert>
#include <iostream>
#include <cmath>

//#define DEBUG_PROPERTY_BOX 1

PropertyBox::PropertyBox(PropertyContainer *container) :
    m_container(container),
    m_showButton(0),
    m_playButton(0)
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
	disconnect(layer, SIGNAL(modelReplaced()),
                   this, SLOT(populateViewPlayFrame()));
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

	m_showButton = new LEDButton(Qt::blue);
	layout->addWidget(m_showButton);
	connect(m_showButton, SIGNAL(stateChanged(bool)),
		this, SIGNAL(showLayer(bool)));
        connect(m_showButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_showButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
	layout->setAlignment(m_showButton, Qt::AlignVCenter);
    }
    
    if (params) {

	QLabel *playLabel = new QLabel(tr("Play"));
	layout->addWidget(playLabel);
	layout->setAlignment(playLabel, Qt::AlignVCenter);

	m_playButton = new LEDButton(Qt::darkGreen);
        m_playButton->setState(!params->isPlayMuted());
	layout->addWidget(m_playButton);
	connect(m_playButton, SIGNAL(stateChanged(bool)),
		params, SLOT(setPlayAudible(bool)));
        connect(m_playButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_playButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
	connect(params, SIGNAL(playAudibleChanged(bool)),
		m_playButton, SLOT(setState(bool)));
	layout->setAlignment(m_playButton, Qt::AlignVCenter);

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
	gainDial->setDefaultValue(0);
        gainDial->setObjectName(tr("Playback Gain"));
        gainDial->setRangeMapper(new LinearRangeMapper
                                 (-50, 50, -25, 25, tr("dB")));
        gainDial->setShowToolTip(true);
	connect(gainDial, SIGNAL(valueChanged(int)),
		this, SLOT(playGainDialChanged(int)));
	connect(params, SIGNAL(playGainChanged(float)),
		this, SLOT(playGainChanged(float)));
	connect(this, SIGNAL(changePlayGain(float)),
		params, SLOT(setPlayGain(float)));
	connect(this, SIGNAL(changePlayGainDial(int)),
		gainDial, SLOT(setValue(int)));
        connect(gainDial, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(gainDial, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
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
        panDial->setObjectName(tr("Playback Pan / Balance"));
        panDial->setShowToolTip(true);
	connect(panDial, SIGNAL(valueChanged(int)),
		this, SLOT(playPanDialChanged(int)));
	connect(params, SIGNAL(playPanChanged(float)),
		this, SLOT(playPanChanged(float)));
	connect(this, SIGNAL(changePlayPan(float)),
		params, SLOT(setPlayPan(float)));
	connect(this, SIGNAL(changePlayPanDial(int)),
		panDial, SLOT(setValue(int)));
        connect(panDial, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(panDial, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
	layout->setAlignment(panDial, Qt::AlignVCenter);

    } else {

	layout->insertStretch(-1, 10);
    }
}

void
PropertyBox::updatePropertyEditor(PropertyContainer::PropertyName name,
                                  bool rangeChanged)
{
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);
    int row = m_layout->rowCount();

    int min = 0, max = 0, value = 0, deflt = 0;
    value = m_container->getPropertyRangeAndValue(name, &min, &max, &deflt);

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
        NotifyingCheckBox *cb;

	if (have) {
	    cb = dynamic_cast<NotifyingCheckBox *>(m_propertyControllers[name]);
	    assert(cb);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: creating new checkbox" << std::endl;
#endif
	    cb = new NotifyingCheckBox();
	    cb->setObjectName(name);
	    connect(cb, SIGNAL(stateChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));
            connect(cb, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(cb, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
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
            if (rangeChanged) {
                dial->blockSignals(true);
                dial->setMinimum(min);
                dial->setMaximum(max);
                dial->setRangeMapper(m_container->getNewPropertyRangeMapper(name));
                dial->blockSignals(false);
            }
                
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
	    dial->setDefaultValue(deflt);
            dial->setRangeMapper(m_container->getNewPropertyRangeMapper(name));
            dial->setShowToolTip(true);
	    connect(dial, SIGNAL(valueChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));
            connect(dial, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(dial, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

	    if (inGroup) {
		dial->setFixedWidth(24);
		dial->setFixedHeight(24);
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
	NotifyingComboBox *cb;

	if (have) {
	    cb = dynamic_cast<NotifyingComboBox *>(m_propertyControllers[name]);
	    assert(cb);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: creating new combobox" << std::endl;
#endif

	    cb = new NotifyingComboBox();
	    cb->setObjectName(name);
            cb->setDuplicatesEnabled(false);
        }

        if (!have || rangeChanged) {
            cb->blockSignals(true);
            cb->clear();
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
            cb->blockSignals(false);
        }

        if (!have) {
	    connect(cb, SIGNAL(activated(int)),
		    this, SLOT(propertyControllerChanged(int)));
            connect(cb, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(cb, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

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
PropertyBox::propertyContainerPropertyRangeChanged(PropertyContainer *pc)
{
    blockSignals(true);

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i], true);
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
        NotifyingComboBox *cb = dynamic_cast<NotifyingComboBox *>(obj);
        if (cb) {
            QString unit = cb->currentText();
            m_container->setPropertyWithCommand
                (name, UnitDatabase::getInstance()->getUnitId(unit));
        }
    } else if (type != PropertyContainer::InvalidProperty) {
	m_container->setPropertyWithCommand(name, value);
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

void
PropertyBox::layerVisibilityChanged(bool visible)
{
    if (m_showButton) m_showButton->setState(visible);
}

void
PropertyBox::mouseEnteredWidget()
{
    QWidget *w = dynamic_cast<QWidget *>(sender());
    if (!w) return;
    
    if (!m_container) return;
    QString cname = m_container->getPropertyContainerName();
    if (cname == "") return;

    QString wname = w->objectName();

    if (w == m_showButton) {
        emit contextHelpChanged(tr("Toggle Visibility of %1").arg(cname));
    } else if (w == m_playButton) {
        emit contextHelpChanged(tr("Toggle Playback of %1").arg(cname));
    } else if (wname == "") {
        return;
    } else if (dynamic_cast<NotifyingCheckBox *>(w)) {
        emit contextHelpChanged(tr("Toggle %1 property of %2")
                                .arg(wname).arg(cname));
    } else {
        emit contextHelpChanged(tr("Adjust %1 property of %2")
                                .arg(wname).arg(cname));
    }
}

void
PropertyBox::mouseLeftWidget()
{
    emit contextHelpChanged("");
}


