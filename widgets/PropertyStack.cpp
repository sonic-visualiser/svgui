/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005
    
    This is experimental software.  Not for distribution.
*/

#include "PropertyStack.h"
#include "PropertyBox.h"
#include "base/PropertyContainer.h"
#include "base/View.h"

#include <QIcon>
#include <QTabWidget>

#include <iostream>

#define DEBUG_PROPERTY_STACK 1

PropertyStack::PropertyStack(QWidget *parent, View *client) :
    QTabWidget(parent),
    m_client(client)
{
    repopulate();

    connect(this, SIGNAL(currentChanged(int)),
	    this, SLOT(selectedContainerChanged(int)));

    connect(m_client, SIGNAL(propertyContainerAdded(PropertyContainer *)),
	    this, SLOT(propertyContainerAdded(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerRemoved(PropertyContainer *)),
	    this, SLOT(propertyContainerRemoved(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerPropertyChanged(PropertyContainer *)),
	    this, SLOT(propertyContainerPropertyChanged(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerNameChanged(PropertyContainer *)),
	    this, SLOT(propertyContainerNameChanged(PropertyContainer *)));

    connect(this, SIGNAL(propertyContainerSelected(PropertyContainer *)),
	    m_client, SLOT(propertyContainerSelected(PropertyContainer *)));
}

void
PropertyStack::repopulate()
{
    blockSignals(true);

#ifdef DEBUG_PROPERTY_STACK
    std::cerr << "PropertyStack::repopulate" << std::endl;
#endif
    
    while (count() > 0) {
	removeTab(0);
    }
    for (size_t i = 0; i < m_boxes.size(); ++i) {
	delete m_boxes[i];
    }
    m_boxes.clear();
    
    for (size_t i = 0; i < m_client->getPropertyContainerCount(); ++i) {

	PropertyContainer *container = m_client->getPropertyContainer(i);
	QString name = container->getPropertyContainerName();
	
	QString iconName = container->getPropertyContainerIconName();

	PropertyBox *box = new PropertyBox(container);

	QIcon icon(QString(":/icons/%1.png").arg(iconName));
	if (icon.isNull()) {
	    addTab(box, name);
	} else {
	    addTab(box, icon, QString("&%1").arg(i + 1));
	    setTabToolTip(count() - 1, name);
	}

	m_boxes.push_back(box);
    }    

    blockSignals(false);
}

bool
PropertyStack::containsContainer(PropertyContainer *pc) const
{
    for (size_t i = 0; i < m_client->getPropertyContainerCount(); ++i) {
	PropertyContainer *container = m_client->getPropertyContainer(i);
	if (pc == container) return true;
    }

    return false;
}

void
PropertyStack::propertyContainerAdded(PropertyContainer *)
{
    if (sender() != m_client) return;
    repopulate();
}

void
PropertyStack::propertyContainerRemoved(PropertyContainer *)
{
    if (sender() != m_client) return;
    repopulate();
}

void
PropertyStack::propertyContainerPropertyChanged(PropertyContainer *pc)
{
    for (unsigned int i = 0; i < m_boxes.size(); ++i) {
	if (pc == m_boxes[i]->getContainer()) {
	    m_boxes[i]->propertyContainerPropertyChanged(pc);
	}
    }
}

void
PropertyStack::propertyContainerNameChanged(PropertyContainer *pc)
{
    if (sender() != m_client) return;
    repopulate();
}

void
PropertyStack::selectedContainerChanged(int n)
{
    if (n >= int(m_boxes.size())) return;
    emit propertyContainerSelected(m_boxes[n]->getContainer());
}

#ifdef INCLUDE_MOCFILES
#include "PropertyStack.moc.cpp"
#endif

