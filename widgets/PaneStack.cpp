
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "PaneStack.h"

#include "widgets/Pane.h"
#include "widgets/PropertyStack.h"
#include "base/Layer.h"
#include "base/ViewManager.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPalette>
#include <QLabel>

#include <iostream>

PaneStack::PaneStack(QWidget *parent, ViewManager *viewManager) :
    QSplitter(parent),
    m_currentPane(0),
    m_viewManager(viewManager)
{
    setOrientation(Qt::Vertical);
    setOpaqueResize(false);
}

Pane *
PaneStack::addPane(bool suppressPropertyBox)
{
    QFrame *frame = new QFrame;
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(2);

    QLabel *currentIndicator = new QLabel(frame);
    currentIndicator->setFixedWidth(QPainter(this).fontMetrics().width("x"));
    layout->addWidget(currentIndicator);
    layout->setStretchFactor(currentIndicator, 1);
    currentIndicator->setScaledContents(true);
    m_currentIndicators.push_back(currentIndicator);

    Pane *pane = new Pane(frame);
    pane->setViewManager(m_viewManager);
    layout->addWidget(pane);
    layout->setStretchFactor(pane, 10);
    m_panes.push_back(pane);

    QWidget *properties = 0;
    if (suppressPropertyBox) {
	properties = new QFrame();
    } else {
	properties = new PropertyStack(frame, pane);
	connect(properties, SIGNAL(propertyContainerSelected(PropertyContainer *)),
		this, SLOT(propertyContainerSelected(PropertyContainer *)));
    }
    layout->addWidget(properties);
    layout->setStretchFactor(properties, 1);
    m_propertyStacks.push_back(properties);

    frame->setLayout(layout);
    addWidget(frame);

    connect(pane, SIGNAL(propertyContainerAdded(PropertyContainer *)),
	    this, SLOT(propertyContainerAdded(PropertyContainer *)));
    connect(pane, SIGNAL(propertyContainerRemoved(PropertyContainer *)),
	    this, SLOT(propertyContainerRemoved(PropertyContainer *)));
    connect(pane, SIGNAL(paneInteractedWith()),
	    this, SLOT(paneInteractedWith()));

    if (!m_currentPane) {
	setCurrentPane(pane);
    }

    return pane;
}

Pane *
PaneStack::getPane(int n)
{
    return m_panes[n];
}

void
PaneStack::deletePane(Pane *pane)
{
    int n = 0;
    std::vector<Pane *>::iterator i = m_panes.begin();
    std::vector<QWidget *>::iterator j = m_propertyStacks.begin();
    std::vector<QLabel *>::iterator k = m_currentIndicators.begin();

    while (i != m_panes.end()) {
	if (*i == pane) break;
	++i;
	++j;
	++k;
	++n;
    }
    if (n >= int(m_panes.size())) return;

    m_panes.erase(i);
    m_propertyStacks.erase(j);
    m_currentIndicators.erase(k);
    delete widget(n);

    if (m_currentPane == pane) {
	if (m_panes.size() > 0) {
	    setCurrentPane(m_panes[0]);
	} else {
	    setCurrentPane(0);
	}
    }
}

int
PaneStack::getPaneCount() const
{
    return m_panes.size();
}

void
PaneStack::setCurrentPane(Pane *pane) // may be null
{
    if (m_currentPane == pane) return;
    
    std::vector<Pane *>::iterator i = m_panes.begin();
    std::vector<QLabel *>::iterator k = m_currentIndicators.begin();

    // We used to do this by setting the foreground and background
    // role, but it seems the background role is ignored and the
    // background drawn transparent in Qt 4.1 -- I can't quite see why
    
    QPixmap selectedMap(1, 1);
    selectedMap.fill(QApplication::palette().color(QPalette::Foreground));
    
    QPixmap unselectedMap(1, 1);
    unselectedMap.fill(QApplication::palette().color(QPalette::Background));

    while (i != m_panes.end()) {
	if (*i == pane) {
	    (*k)->setPixmap(selectedMap);
	} else {
	    (*k)->setPixmap(unselectedMap);
	}
	++i;
	++k;
    }
    m_currentPane = pane;

    emit currentPaneChanged(m_currentPane);
}

Pane *
PaneStack::getCurrentPane() 
{
    return m_currentPane;
}

void
PaneStack::propertyContainerAdded(PropertyContainer *)
{
    sizePropertyStacks();
}

void
PaneStack::propertyContainerRemoved(PropertyContainer *)
{
    sizePropertyStacks();
}

void
PaneStack::propertyContainerSelected(PropertyContainer *pc)
{
    std::vector<Pane *>::iterator i = m_panes.begin();
    std::vector<QWidget *>::iterator j = m_propertyStacks.begin();

    while (i != m_panes.end()) {
	PropertyStack *stack = dynamic_cast<PropertyStack *>(*j);
	if (stack && stack->containsContainer(pc)) {
	    setCurrentPane(*i);
	    break;
	}
	++i;
	++j;
    }
}

void
PaneStack::paneInteractedWith()
{
    Pane *pane = dynamic_cast<Pane *>(sender());
    if (!pane) return;
    setCurrentPane(pane);
}

void
PaneStack::sizePropertyStacks()
{
    int maxMinWidth = 0;

    for (unsigned int i = 0; i < m_propertyStacks.size(); ++i) {
	if (!m_propertyStacks[i]) continue;
	std::cerr << "PaneStack::sizePropertyStacks: " << i << ": min " 
		  << m_propertyStacks[i]->minimumSizeHint().width() << ", current "
		  << m_propertyStacks[i]->width() << std::endl;

	if (m_propertyStacks[i]->minimumSizeHint().width() > maxMinWidth) {
	    maxMinWidth = m_propertyStacks[i]->minimumSizeHint().width();
	}
    }

    std::cerr << "PaneStack::sizePropertyStacks: max min width " << maxMinWidth << std::endl;

#ifdef Q_WS_MAC
    // This is necessary to compensate for cb->setMinimumSize(10, 10)
    // in PropertyBox in the Mac version (to avoid a mysterious crash)
    int setWidth = maxMinWidth * 3 / 2;
#else
    int setWidth = maxMinWidth;
#endif

    for (unsigned int i = 0; i < m_propertyStacks.size(); ++i) {
	if (!m_propertyStacks[i]) continue;
	m_propertyStacks[i]->setMinimumWidth(setWidth);
    }
}
    

#ifdef INCLUDE_MOCFILES
#include "PaneStack.moc.cpp"
#endif

