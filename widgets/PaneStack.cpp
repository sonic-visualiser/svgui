
/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

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

    Pane *pane = new Pane(frame);
    pane->setViewManager(m_viewManager);
    layout->addWidget(pane);
    layout->setStretchFactor(pane, 10);

    QWidget *properties = 0;
    if (suppressPropertyBox) {
	properties = new QFrame();
    } else {
	properties = new PropertyStack(frame, pane);
	connect(properties, SIGNAL(propertyContainerSelected(View *, PropertyContainer *)),
		this, SLOT(propertyContainerSelected(View *, PropertyContainer *)));
    }
    layout->addWidget(properties);
    layout->setStretchFactor(properties, 1);

    PaneRec rec;
    rec.pane = pane;
    rec.propertyStack = properties;
    rec.currentIndicator = currentIndicator;
    m_panes.push_back(rec);

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
    return m_panes[n].pane;
}

Pane *
PaneStack::getHiddenPane(int n)
{
    return m_hiddenPanes[n].pane;
}

void
PaneStack::deletePane(Pane *pane)
{
    std::vector<PaneRec>::iterator i;
    bool found = false;

    for (i = m_panes.begin(); i != m_panes.end(); ++i) {
	if (i->pane == pane) {
	    m_panes.erase(i);
	    found = true;
	    break;
	}
    }

    if (!found) {

	for (i = m_hiddenPanes.begin(); i != m_hiddenPanes.end(); ++i) {
	    if (i->pane == pane) {
		m_hiddenPanes.erase(i);
		found = true;
		break;
	    }
	}

	if (!found) {
	    std::cerr << "WARNING: PaneStack::deletePane(" << pane << "): Pane not found in visible or hidden panes, not deleting" << std::endl;
	    return;
	}
    }

    delete pane->parent();

    if (m_currentPane == pane) {
	if (m_panes.size() > 0) {
	    setCurrentPane(m_panes[0].pane);
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

int
PaneStack::getHiddenPaneCount() const
{
    return m_hiddenPanes.size();
}

void
PaneStack::hidePane(Pane *pane)
{
    std::vector<PaneRec>::iterator i = m_panes.begin();

    while (i != m_panes.end()) {
	if (i->pane == pane) {

	    m_hiddenPanes.push_back(*i);
	    m_panes.erase(i);

	    QWidget *pw = dynamic_cast<QWidget *>(pane->parent());
	    if (pw) pw->hide();

	    if (m_currentPane == pane) {
		if (m_panes.size() > 0) {
		    setCurrentPane(m_panes[0].pane);
		} else {
		    setCurrentPane(0);
		}
	    }
	    
	    return;
	}
	++i;
    }

    std::cerr << "WARNING: PaneStack::hidePane(" << pane << "): Pane not found in visible panes" << std::endl;
}

void
PaneStack::showPane(Pane *pane)
{
    std::vector<PaneRec>::iterator i = m_hiddenPanes.begin();

    while (i != m_hiddenPanes.end()) {
	if (i->pane == pane) {
	    m_panes.push_back(*i);
	    m_hiddenPanes.erase(i);
	    QWidget *pw = dynamic_cast<QWidget *>(pane->parent());
	    if (pw) pw->show();

	    //!!! update current pane

	    return;
	}
	++i;
    }

    std::cerr << "WARNING: PaneStack::showPane(" << pane << "): Pane not found in hidden panes" << std::endl;
}

void
PaneStack::setCurrentPane(Pane *pane) // may be null
{
    if (m_currentPane == pane) return;
    
    std::vector<PaneRec>::iterator i = m_panes.begin();

    // We used to do this by setting the foreground and background
    // role, but it seems the background role is ignored and the
    // background drawn transparent in Qt 4.1 -- I can't quite see why
    
    QPixmap selectedMap(1, 1);
    selectedMap.fill(QApplication::palette().color(QPalette::Foreground));
    
    QPixmap unselectedMap(1, 1);
    unselectedMap.fill(QApplication::palette().color(QPalette::Background));

    bool found = false;

    while (i != m_panes.end()) {
	if (i->pane == pane) {
	    i->currentIndicator->setPixmap(selectedMap);
	    found = true;
	} else {
	    i->currentIndicator->setPixmap(unselectedMap);
	}
	++i;
    }

    if (found || pane == 0) {
	m_currentPane = pane;
	emit currentPaneChanged(m_currentPane);
    } else {
	std::cerr << "WARNING: PaneStack::setCurrentPane(" << pane << "): pane is not a visible pane in this stack" << std::endl;
    }
}

void
PaneStack::setCurrentLayer(Pane *pane, Layer *layer) // may be null
{
    setCurrentPane(pane);

    if (m_currentPane) {

	std::vector<PaneRec>::iterator i = m_panes.begin();

	while (i != m_panes.end()) {

	    if (i->pane == pane) {
		PropertyStack *stack = dynamic_cast<PropertyStack *>
		    (i->propertyStack);
		if (stack) {
		    if (stack->containsContainer(layer)) {
			stack->setCurrentIndex(stack->getContainerIndex(layer));
			emit currentLayerChanged(pane, layer);
		    } else {
			stack->setCurrentIndex
			    (stack->getContainerIndex
			     (pane->getPropertyContainer(0)));
			emit currentLayerChanged(pane, 0);
		    }
		}
		break;
	    }
	    ++i;
	}
    }
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
PaneStack::propertyContainerSelected(View *client, PropertyContainer *pc)
{
    std::vector<PaneRec>::iterator i = m_panes.begin();

    while (i != m_panes.end()) {
	PropertyStack *stack = dynamic_cast<PropertyStack *>(i->propertyStack);
	if (stack &&
	    stack->getClient() == client &&
	    stack->containsContainer(pc)) {
	    setCurrentPane(i->pane);
	    break;
	}
	++i;
    }

    Layer *layer = dynamic_cast<Layer *>(pc);
    if (layer) emit currentLayerChanged(m_currentPane, layer);
    else emit currentLayerChanged(m_currentPane, 0);
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

    for (size_t i = 0; i < m_panes.size(); ++i) {
	if (!m_panes[i].propertyStack) continue;
	std::cerr << "PaneStack::sizePropertyStacks: " << i << ": min " 
		  << m_panes[i].propertyStack->minimumSizeHint().width() << ", current "
		  << m_panes[i].propertyStack->width() << std::endl;

	if (m_panes[i].propertyStack->minimumSizeHint().width() > maxMinWidth) {
	    maxMinWidth = m_panes[i].propertyStack->minimumSizeHint().width();
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

    for (size_t i = 0; i < m_panes.size(); ++i) {
	if (!m_panes[i].propertyStack) continue;
	m_panes[i].propertyStack->setMinimumWidth(setWidth);
    }
}
    

#ifdef INCLUDE_MOCFILES
#include "PaneStack.moc.cpp"
#endif

