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

#include "PropertyStack.h"
#include "PropertyBox.h"
#include "base/PropertyContainer.h"
#include "view/View.h"
#include "layer/Layer.h"
#include "widgets/NotifyingTabBar.h"

#include <QIcon>
#include <QTabWidget>

#include <iostream>

//#define DEBUG_PROPERTY_STACK 1

PropertyStack::PropertyStack(QWidget *parent, View *client) :
    QTabWidget(parent),
    m_client(client)
{
    NotifyingTabBar *bar = new NotifyingTabBar();
    bar->setDrawBase(false);

    connect(bar, SIGNAL(mouseEntered()), this, SLOT(mouseEnteredTabBar()));
    connect(bar, SIGNAL(mouseLeft()), this, SLOT(mouseLeftTabBar()));
    connect(bar, SIGNAL(activeTabClicked()), this, SLOT(activeTabClicked()));

    setTabBar(bar);

    repopulate();

    connect(this, SIGNAL(currentChanged(int)),
	    this, SLOT(selectedContainerChanged(int)));

    connect(m_client, SIGNAL(propertyContainerAdded(PropertyContainer *)),
	    this, SLOT(propertyContainerAdded(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerRemoved(PropertyContainer *)),
	    this, SLOT(propertyContainerRemoved(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerPropertyChanged(PropertyContainer *)),
	    this, SLOT(propertyContainerPropertyChanged(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerPropertyRangeChanged(PropertyContainer *)),
	    this, SLOT(propertyContainerPropertyRangeChanged(PropertyContainer *)));

    connect(m_client, SIGNAL(propertyContainerNameChanged(PropertyContainer *)),
	    this, SLOT(propertyContainerNameChanged(PropertyContainer *)));

    connect(this, SIGNAL(propertyContainerSelected(View *, PropertyContainer *)),
	    m_client, SLOT(propertyContainerSelected(View *, PropertyContainer *)));
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

	connect(box, SIGNAL(showLayer(bool)), this, SLOT(showLayer(bool)));
        connect(box, SIGNAL(contextHelpChanged(const QString &)),
                this, SIGNAL(contextHelpChanged(const QString &)));

        Layer *layer = dynamic_cast<Layer *>(container);
        if (layer) {
            box->layerVisibilityChanged(!layer->isLayerDormant(m_client));
        }

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

int
PropertyStack::getContainerIndex(PropertyContainer *pc) const
{
    for (size_t i = 0; i < m_client->getPropertyContainerCount(); ++i) {
	PropertyContainer *container = m_client->getPropertyContainer(i);
	if (pc == container) return i;
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
PropertyStack::propertyContainerPropertyRangeChanged(PropertyContainer *pc)
{
    for (unsigned int i = 0; i < m_boxes.size(); ++i) {
	if (pc == m_boxes[i]->getContainer()) {
	    m_boxes[i]->propertyContainerPropertyRangeChanged(pc);
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
PropertyStack::showLayer(bool show)
{
    QObject *obj = sender();
    
    for (unsigned int i = 0; i < m_boxes.size(); ++i) {
	if (obj == m_boxes[i]) {
	    Layer *layer = dynamic_cast<Layer *>(m_boxes[i]->getContainer());
	    if (layer) {
		layer->showLayer(m_client, show);
		return;
	    }
	}
    }
}

void
PropertyStack::selectedContainerChanged(int n)
{
    if (n >= int(m_boxes.size())) return;
    emit propertyContainerSelected(m_client, m_boxes[n]->getContainer());
}

void
PropertyStack::mouseEnteredTabBar()
{
    emit contextHelpChanged(tr("Click to change the current active layer"));
}

void
PropertyStack::mouseLeftTabBar()
{
    emit contextHelpChanged("");
}

void
PropertyStack::activeTabClicked()
{
    emit viewSelected(m_client);
}

