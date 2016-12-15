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
#include "base/PlayParameterRepository.h"
#include "layer/Layer.h"
#include "layer/ColourDatabase.h"
#include "base/UnitDatabase.h"
#include "base/RangeMapper.h"

#include "AudioDial.h"
#include "LEDButton.h"
#include "IconLoader.h"
#include "LevelPanWidget.h"
#include "LevelPanToolButton.h"
#include "WidgetScale.h"

#include "NotifyingCheckBox.h"
#include "NotifyingComboBox.h"
#include "NotifyingPushButton.h"
#include "NotifyingToolButton.h"
#include "ColourNameDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QApplication>
#include <QColorDialog>
#include <QInputDialog>
#include <QDir>

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
    cerr << "PropertyBox[" << this << "(\"" <<
	container->getPropertyContainerName() << "\" at " << container << ")]::PropertyBox" << endl;
#endif

    m_mainBox = new QVBoxLayout;
    setLayout(m_mainBox);

#ifdef Q_OS_MAC
    QMargins mm = m_mainBox->contentsMargins();
    QMargins mmhalf(mm.left()/2, mm.top()/3, mm.right()/2, mm.bottom()/3);
    m_mainBox->setContentsMargins(mmhalf);
#endif

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
    m_layout->setHorizontalSpacing(2);
    m_layout->setVerticalSpacing(1);
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

    connect(ColourDatabase::getInstance(), SIGNAL(colourDatabaseChanged()),
            this, SLOT(colourDatabaseChanged()));

#ifdef DEBUG_PROPERTY_BOX
    cerr << "PropertyBox[" << this << "]::PropertyBox returning" << endl;
#endif
}

PropertyBox::~PropertyBox()
{
#ifdef DEBUG_PROPERTY_BOX
    cerr << "PropertyBox[" << this << "]::~PropertyBox" << endl;
#endif
}

void
PropertyBox::populateViewPlayFrame()
{
#ifdef DEBUG_PROPERTY_BOX
    cerr << "PropertyBox[" << this << ":" << m_container << "]::populateViewPlayFrame" << endl;
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

    QGridLayout *layout = new QGridLayout;
    m_viewPlayFrame->setLayout(layout);

    layout->setMargin(layout->margin() / 2);

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::populateViewPlayFrame: container " << m_container << " (name " << m_container->getPropertyContainerName() << ") params " << params << endl;
#endif

    QSize buttonSize = WidgetScale::scaleQSize(QSize(26, 26));
    int col = 0;

    if (params) {
        
        m_playButton = new NotifyingToolButton;
        m_playButton->setCheckable(true);
        m_playButton->setIcon(IconLoader().load("speaker"));
        m_playButton->setToolTip(tr("Click to toggle playback"));
        m_playButton->setChecked(!params->isPlayMuted());
        m_playButton->setFixedSize(buttonSize);
	connect(m_playButton, SIGNAL(toggled(bool)),
		this, SLOT(playAudibleButtonChanged(bool)));
        connect(m_playButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_playButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
	connect(params, SIGNAL(playAudibleChanged(bool)),
		this, SLOT(playAudibleChanged(bool)));

        LevelPanToolButton *levelPan = new LevelPanToolButton;
        levelPan->setFixedSize(buttonSize);
        levelPan->setImageSize((buttonSize.height() * 3) / 4);
        layout->addWidget(levelPan, 0, col++, Qt::AlignCenter);
        connect(levelPan, SIGNAL(levelChanged(float)),
                this, SLOT(playGainControlChanged(float)));
        connect(levelPan, SIGNAL(panChanged(float)),
                this, SLOT(playPanControlChanged(float)));
        connect(params, SIGNAL(playGainChanged(float)),
                levelPan, SLOT(setLevel(float)));
        connect(params, SIGNAL(playPanChanged(float)),
                levelPan, SLOT(setPan(float)));
        connect(levelPan, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(levelPan, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));

	layout->addWidget(m_playButton, 0, col++, Qt::AlignCenter);

        if (params->getPlayClipId() != "") {
            QToolButton *playParamButton = new QToolButton;
            playParamButton->setObjectName("playParamButton");
            playParamButton->setIcon(IconLoader().load("faders"));
            playParamButton->setFixedSize(buttonSize);
            layout->addWidget(playParamButton, 0, col++, Qt::AlignCenter);
            connect(playParamButton, SIGNAL(clicked()),
                    this, SLOT(editPlayParameters()));
            connect(playParamButton, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(playParamButton, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
        }
    }

    layout->setColumnStretch(col++, 10);

    if (layer) {

	QLabel *showLabel = new QLabel(tr("Show"));
	layout->addWidget(showLabel, 0, col++, Qt::AlignVCenter | Qt::AlignRight);

	m_showButton = new LEDButton(Qt::blue);
	layout->addWidget(m_showButton, 0, col++, Qt::AlignVCenter | Qt::AlignLeft);
	connect(m_showButton, SIGNAL(stateChanged(bool)),
		this, SIGNAL(showLayer(bool)));
        connect(m_showButton, SIGNAL(mouseEntered()),
                this, SLOT(mouseEnteredWidget()));
        connect(m_showButton, SIGNAL(mouseLeft()),
                this, SLOT(mouseLeftWidget()));
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
    QString iconName = m_container->getPropertyIconName(name);

#ifdef DEBUG_PROPERTY_BOX
    cerr << "PropertyBox[" << this
	      << "(\"" << m_container->getPropertyContainerName()
	      << "\")]";
    cerr << "::updatePropertyEditor(\"" << name << "\"):";
    cerr << " value " << value << ", have " << have << ", group \""
	      << groupName << "\"" << endl;
#endif

    bool inGroup = (groupName != QString());

    if (!have) {

        QLabel *labelWidget = 0;

	if (inGroup) {
	    if (m_groupLayouts.find(groupName) == m_groupLayouts.end()) {
                labelWidget = new QLabel(groupName, m_mainWidget);
            }
        } else {
            inGroup = true;
            groupName = "ungrouped: " + propertyLabel;
	    if (m_groupLayouts.find(groupName) == m_groupLayouts.end()) {
                labelWidget = new QLabel(propertyLabel, m_mainWidget);
            }
        }
        
        if (labelWidget) {
            m_layout->addWidget(labelWidget, row, 0);
            QWidget *frame = new QWidget(m_mainWidget);
            frame->setMinimumSize(WidgetScale::scaleQSize(QSize(1, 24)));
            m_groupLayouts[groupName] = new QGridLayout;
            m_groupLayouts[groupName]->setMargin(0);
            frame->setLayout(m_groupLayouts[groupName]);
            m_layout->addWidget(frame, row, 1, 1, 2);
            m_layout->setColumnStretch(1, 10);
        }
    }

    switch (type) {

    case PropertyContainer::ToggleProperty:
    {
        QAbstractButton *button = 0;

	if (have) {
            button = dynamic_cast<QAbstractButton *>(m_propertyControllers[name]);
            assert(button);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    cerr << "PropertyBox: creating new checkbox" << endl;
#endif
            if (iconName != "") {
                button = new NotifyingPushButton();
                button->setCheckable(true);
                QIcon icon(IconLoader().load(iconName));
                button->setIcon(icon);
                button->setObjectName(name);
                button->setFixedSize(WidgetScale::scaleQSize(QSize(18, 18)));
            } else {
                button = new NotifyingCheckBox();
                button->setObjectName(name);
            }
	    connect(button, SIGNAL(toggled(bool)),
		    this, SLOT(propertyControllerChanged(bool)));
            connect(button, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(button, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));
	    if (inGroup) {
		button->setToolTip(propertyLabel);
		m_groupLayouts[groupName]->addWidget
                    (button, 0, m_groupLayouts[groupName]->columnCount());
	    } else {
		m_layout->addWidget(button, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = button;
	}

        if (button->isChecked() != (value > 0)) {
	    button->blockSignals(true);
	    button->setChecked(value > 0);
	    button->blockSignals(false);
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
	    cerr << "PropertyBox: creating new dial" << endl;
#endif
	    dial = new AudioDial();
	    dial->setObjectName(name);
	    dial->setMinimum(min);
	    dial->setMaximum(max);
	    dial->setPageStep(1);
	    dial->setNotchesVisible((max - min) <= 12);
            // important to set the range mapper before the default,
            // because the range mapper is used to map the default
            dial->setRangeMapper(m_container->getNewPropertyRangeMapper(name));
	    dial->setDefaultValue(deflt);
            dial->setShowToolTip(true);
	    connect(dial, SIGNAL(valueChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));
            connect(dial, SIGNAL(mouseEntered()),
                    this, SLOT(mouseEnteredWidget()));
            connect(dial, SIGNAL(mouseLeft()),
                    this, SLOT(mouseLeftWidget()));

	    if (inGroup) {
		dial->setFixedWidth(WidgetScale::scalePixelSize(24));
		dial->setFixedHeight(WidgetScale::scalePixelSize(24));
		m_groupLayouts[groupName]->addWidget
                    (dial, 0, m_groupLayouts[groupName]->columnCount());
	    } else {
		dial->setFixedWidth(WidgetScale::scalePixelSize(32));
		dial->setFixedHeight(WidgetScale::scalePixelSize(32));
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
    case PropertyContainer::ColourProperty:
    {
	NotifyingComboBox *cb;

	if (have) {
	    cb = dynamic_cast<NotifyingComboBox *>(m_propertyControllers[name]);
	    assert(cb);
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    cerr << "PropertyBox: creating new combobox" << endl;
#endif

	    cb = new NotifyingComboBox();
	    cb->setObjectName(name);
            cb->setDuplicatesEnabled(false);
        }

        if (!have || rangeChanged) {

            cb->blockSignals(true);
            cb->clear();
            cb->setEditable(false);

            if (type == PropertyContainer::ValueProperty) {

                for (int i = min; i <= max; ++i) {

                    QString label = m_container->getPropertyValueLabel(name, i);
                    QString iname = m_container->getPropertyValueIconName(name, i);

                    if (iname != "") {
                        QIcon icon(IconLoader().load(iname));
                        cb->addItem(icon, label);
                    } else {
                        cb->addItem(label);
                    }
                }

            } else if (type == PropertyContainer::UnitsProperty) {

                QStringList units = UnitDatabase::getInstance()->getKnownUnits();
                for (int i = 0; i < units.size(); ++i) {
                    cb->addItem(units[i]);
                }

                cb->setEditable(true);

            } else { // ColourProperty

                //!!! should be a proper colour combobox class that
                // manages its own Add New Colour entry...

                int size = (QFontMetrics(QFont()).height() * 2) / 3;
                if (size < 12) size = 12;
                
                ColourDatabase *db = ColourDatabase::getInstance();
                for (int i = 0; i < db->getColourCount(); ++i) {
                    QString name = db->getColourName(i);
                    cb->addItem(db->getExamplePixmap(i, QSize(size, size)), name);
                }
                cb->addItem(tr("Add New Colour..."));
            }                
                
            cb->blockSignals(false);
            if (cb->count() < 20 && cb->count() > cb->maxVisibleItems()) {
                cb->setMaxVisibleItems(cb->count());
            }
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
		m_groupLayouts[groupName]->addWidget
                    (cb, 0, m_groupLayouts[groupName]->columnCount());
	    } else {
		m_layout->addWidget(cb, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = cb;
	}

        cb->blockSignals(true);
        if (type == PropertyContainer::ValueProperty ||
            type == PropertyContainer::ColourProperty) {
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

#ifdef Q_OS_MAC
	// Crashes on startup without this, for some reason; also
	// prevents combo boxes from getting weirdly squished
	// vertically
	cb->setMinimumSize(QSize(10, cb->font().pixelSize() * 2));
#endif

	break;
    }

    case PropertyContainer::InvalidProperty:
    default:
	break;
    }
}

void
PropertyBox::propertyContainerPropertyChanged(PropertyContainer *pc)
{
    if (pc != m_container) return;
    
#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::propertyContainerPropertyChanged" << endl;
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
PropertyBox::propertyContainerPropertyRangeChanged(PropertyContainer *)
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
#ifdef DEBUG_PROPERTY_BOX
    cerr << "PropertyBox[" << this << "]: unitDatabaseChanged" << endl;
#endif
    blockSignals(true);

//    cerr << "my container is " << m_container << endl;
//    cerr << "my container's name is... " << endl;
//    cerr << m_container->objectName() << endl;

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
        if (m_container->getPropertyType(properties[i]) ==
            PropertyContainer::UnitsProperty) {
            updatePropertyEditor(properties[i]);
        }
    }

    blockSignals(false);
}    

void
PropertyBox::colourDatabaseChanged()
{
    blockSignals(true);

    PropertyContainer::PropertyList properties = m_container->getProperties();
    for (size_t i = 0; i < properties.size(); ++i) {
        if (m_container->getPropertyType(properties[i]) ==
            PropertyContainer::ColourProperty) {
            updatePropertyEditor(properties[i], true);
        }
    }

    blockSignals(false);
}    

void
PropertyBox::propertyControllerChanged(bool on)
{
    propertyControllerChanged(on ? 1 : 0);
}

void
PropertyBox::propertyControllerChanged(int value)
{
    QObject *obj = sender();
    QString name = obj->objectName();

#ifdef DEBUG_PROPERTY_BOX
    SVDEBUG << "PropertyBox::propertyControllerChanged(" << name	      << ", " << value << ")" << endl;
#endif
    
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);

    Command *c = 0;

    if (type == PropertyContainer::UnitsProperty) {

        NotifyingComboBox *cb = dynamic_cast<NotifyingComboBox *>(obj);
        if (cb) {
            QString unit = cb->currentText();
            c = m_container->getSetPropertyCommand
                (name, UnitDatabase::getInstance()->getUnitId(unit));
        }

    } else if (type == PropertyContainer::ColourProperty) {

        if (value == int(ColourDatabase::getInstance()->getColourCount())) {
            addNewColour();
            if (value == int(ColourDatabase::getInstance()->getColourCount())) {
                propertyContainerPropertyChanged(m_container);
                return;
            }
        }
        c = m_container->getSetPropertyCommand(name, value);

    } else if (type != PropertyContainer::InvalidProperty) {

	c = m_container->getSetPropertyCommand(name, value);
    }

    if (c) CommandHistory::getInstance()->addCommand(c, true, true);
    
    updateContextHelp(obj);
}

void
PropertyBox::addNewColour()
{
    QColor newColour = QColorDialog::getColor();
    if (!newColour.isValid()) return;

    ColourNameDialog dialog(tr("Name New Colour"),
                            tr("Enter a name for the new colour:"),
                            newColour, newColour.name(), this);
    dialog.showDarkBackgroundCheckbox(tr("Prefer black background for this colour"));
    if (dialog.exec() == QDialog::Accepted) {
        //!!! command
        ColourDatabase *db = ColourDatabase::getInstance();
        int index = db->addColour(newColour, dialog.getColourName());
        db->setUseDarkBackground(index, dialog.isDarkBackgroundChecked());
    }
}

void
PropertyBox::playAudibleChanged(bool audible)
{
    m_playButton->setChecked(audible);
}

void
PropertyBox::playAudibleButtonChanged(bool audible)
{
    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    if (params->isPlayAudible() != audible) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayAudible(audible);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }
}

void
PropertyBox::playGainControlChanged(float gain)
{
    QObject *obj = sender();

    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    if (params->getPlayGain() != gain) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayGain(gain);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }

    updateContextHelp(obj);
}

void
PropertyBox::playPanControlChanged(float pan)
{
    QObject *obj = sender();

    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    if (params->getPlayPan() != pan) {
        PlayParameterRepository::EditCommand *command =
            new PlayParameterRepository::EditCommand(params);
        command->setPlayPan(pan);
        CommandHistory::getInstance()->addCommand(command, true, true);
    }

    updateContextHelp(obj);
}

void
PropertyBox::editPlayParameters()
{
    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    QString clip = params->getPlayClipId();

    PlayParameterRepository::EditCommand *command = 
        new PlayParameterRepository::EditCommand(params);
    
    QInputDialog *dialog = new QInputDialog(this);

    QDir dir(":/samples");
    QStringList clipFiles = dir.entryList(QStringList() << "*.wav", QDir::Files);

    QStringList clips;
    foreach (QString str, clipFiles) {
        clips.push_back(str.replace(".wav", ""));
    }
    dialog->setComboBoxItems(clips);

    dialog->setLabelText(tr("Set playback clip:"));

    QComboBox *cb = dialog->findChild<QComboBox *>();
    if (cb) {
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemText(i) == clip) {
                cb->setCurrentIndex(i);
            }
        }
    }

    connect(dialog, SIGNAL(textValueChanged(QString)), 
            this, SLOT(playClipChanged(QString)));

    if (dialog->exec() == QDialog::Accepted) {
        QString newClip = dialog->textValue();
        command->setPlayClipId(newClip);
        CommandHistory::getInstance()->addCommand(command, true);
    } else {
        delete command;
        // restore in case we mucked about with the configuration
        // as a consequence of signals from the dialog
        params->setPlayClipId(clip);
    }

    delete dialog;
}

void
PropertyBox::playClipChanged(QString id)
{
    PlayParameters *params = m_container->getPlayParameters();
    if (!params) return;

    params->setPlayClipId(id);
}    

void
PropertyBox::layerVisibilityChanged(bool visible)
{
    if (m_showButton) m_showButton->setState(visible);
}

void
PropertyBox::mouseEnteredWidget()
{
    updateContextHelp(sender());
}

void
PropertyBox::updateContextHelp(QObject *o)
{
    QWidget *w = dynamic_cast<QWidget *>(o);
    if (!w) return;

    if (!m_container) return;
    QString cname = m_container->getPropertyContainerName();
    if (cname == "") return;

    LevelPanToolButton *lp = qobject_cast<LevelPanToolButton *>(w);
    if (lp) {
        emit contextHelpChanged(tr("Adjust playback level and pan of %1").arg(cname));
        return;
    }

    QString wname = w->objectName();

    if (wname == "playParamButton") {
        PlayParameters *params = m_container->getPlayParameters();
        if (params) {
            emit contextHelpChanged
                (tr("Change sound used for playback (currently \"%1\")")
                 .arg(params->getPlayClipId()));
            return;
        }
    }
    
    QString extraText;
    
    AudioDial *dial = qobject_cast<AudioDial *>(w);
    if (dial) {
        double mv = dial->mappedValue();
        QString unit = "";
        if (dial->rangeMapper()) unit = dial->rangeMapper()->getUnit();
        if (unit != "") {
            extraText = tr(" (current value: %1%2)").arg(mv).arg(unit);
        } else {
            extraText = tr(" (current value: %1)").arg(mv);
        }
    }

    if (w == m_showButton) {
        emit contextHelpChanged(tr("Toggle Visibility of %1").arg(cname));
    } else if (w == m_playButton) {
        emit contextHelpChanged(tr("Toggle Playback of %1").arg(cname));
    } else if (wname == "") {
        return;
    } else if (dynamic_cast<QAbstractButton *>(w)) {
        emit contextHelpChanged(tr("Toggle %1 property of %2")
                                .arg(wname).arg(cname));
    } else {
        emit contextHelpChanged(tr("Adjust %1 property of %2%3")
                                .arg(wname).arg(cname).arg(extraText));
    }
}

void
PropertyBox::mouseLeftWidget()
{
    if (!(QApplication::mouseButtons() & Qt::LeftButton)) {
        emit contextHelpChanged("");
    }
}


