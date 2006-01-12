/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "PropertyBox.h"

#include "base/PropertyContainer.h"

#include "AudioDial.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>

#include <cassert>
#include <iostream>

//#define DEBUG_PROPERTY_BOX 1

PropertyBox::PropertyBox(PropertyContainer *container) :
    m_container(container)
{
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "(\"" <<
	container->getPropertyContainerName().toStdString() << "\")]::PropertyBox" << std::endl;
#endif

    m_layout = new QGridLayout;
    setLayout(m_layout);

    PropertyContainer::PropertyList properties = container->getProperties();

    blockSignals(true);

    size_t i;

    for (i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i]);
    }

    blockSignals(false);

    m_layout->setRowStretch(m_layout->rowCount(), 10);

#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "]::PropertyBox returning" << std::endl;
#endif
}

PropertyBox::~PropertyBox()
{
#ifdef DEBUG_PROPERTY_BOX
    std::cerr << "PropertyBox[" << this << "(\"" << m_container->getPropertyContainerName().toStdString() << "\")]::~PropertyBox" << std::endl;
#endif
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
		m_layout->addWidget(new QLabel(groupName, this), row, 0);
		QFrame *frame = new QFrame(this);
		m_layout->addWidget(frame, row, 1, 1, 2);
		m_groupLayouts[groupName] = new QHBoxLayout;
		m_groupLayouts[groupName]->setMargin(0);
		frame->setLayout(m_groupLayouts[groupName]);
	    }
	} else {
#ifdef DEBUG_PROPERTY_BOX 
	    std::cerr << "PropertyBox: adding label \"" << name.toStdString() << "\"" << std::endl;
#endif
	    m_layout->addWidget(new QLabel(name, this), row, 0);
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
		cb->setToolTip(name);
		m_groupLayouts[groupName]->addWidget(cb);
	    } else {
		m_layout->addWidget(cb, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = cb;
	}

	if (cb->isChecked() != (value > 0)) cb->setChecked(value > 0);
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
	    dial->setNotchesVisible(true);
	    connect(dial, SIGNAL(valueChanged(int)),
		    this, SLOT(propertyControllerChanged(int)));

	    if (inGroup) {
		dial->setFixedWidth(24);
		dial->setFixedHeight(24);
		dial->setToolTip(name);
		m_groupLayouts[groupName]->addWidget(dial);
	    } else {
		dial->setFixedWidth(32);
		dial->setFixedHeight(32);
		m_layout->addWidget(dial, row, 1);
		QLabel *label = new QLabel(this);
		connect(dial, SIGNAL(valueChanged(int)),
			label, SLOT(setNum(int)));
		label->setNum(value);
		m_layout->addWidget(label, row, 2);
	    }

	    m_propertyControllers[name] = dial;
	}

	if (dial->value() != value) dial->setValue(value);
	break;
    }

    case PropertyContainer::ValueProperty:
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
	    for (int i = min; i <= max; ++i) {
		cb->addItem(m_container->getPropertyValueLabel(name, i));
	    }
	    connect(cb, SIGNAL(activated(int)),
		    this, SLOT(propertyControllerChanged(int)));
	    if (inGroup) {
		cb->setToolTip(name);
		m_groupLayouts[groupName]->addWidget(cb);
	    } else {
		m_layout->addWidget(cb, row, 1, 1, 2);
	    }
	    m_propertyControllers[name] = cb;
	}

	if (cb->currentIndex() != value) cb->setCurrentIndex(value);

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
    
    PropertyContainer::PropertyList properties = m_container->getProperties();
    size_t i;

    blockSignals(true);

    for (i = 0; i < properties.size(); ++i) {
	updatePropertyEditor(properties[i]);
    }

    blockSignals(false);
}

void
PropertyBox::propertyControllerChanged(int value)
{
    QObject *obj = sender();
    QString name = obj->objectName();

//    std::cerr << "PropertyBox::propertyControllerChanged(" << name.toStdString()
//	      << ", " << value << ")" << std::endl;
    
    PropertyContainer::PropertyType type = m_container->getPropertyType(name);
    
    if (type != PropertyContainer::InvalidProperty) {
	m_container->setProperty(name, value);
    }

    if (type == PropertyContainer::RangeProperty) {
	AudioDial *dial = dynamic_cast<AudioDial *>(m_propertyControllers[name]);
	if (dial) {
	    dial->setToolTip(QString("%1: %2").arg(name).arg(value));
	    //!!! unfortunately this doesn't update an already-visible tooltip
	}
    }
}
    
	    

#ifdef INCLUDE_MOCFILES
#include "PropertyBox.moc.cpp"
#endif

