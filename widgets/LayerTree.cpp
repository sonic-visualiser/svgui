
/* -*- c-basic-offset: 4 -*-  vi:set ts=8 sts=4 sw=4: */

/*
    A waveform viewer and audio annotation editor.
    Chris Cannam, Queen Mary University of London, 2005-2006
    
    This is experimental software.  Not for distribution.
*/

#include "LayerTree.h"
#include "PaneStack.h"

#include "widgets/Pane.h"
#include "base/Layer.h"

#include <iostream>


LayerTreeModel::LayerTreeModel(PaneStack *stack, QObject *parent) :
    QAbstractItemModel(parent),
    m_stack(stack)
{
}

LayerTreeModel::~LayerTreeModel()
{
}

QVariant
LayerTreeModel::data(const QModelIndex &index, int role) const
{
    std::cerr << "LayerTreeModel::data(" << &index << ", role " << role << ")" << std::endl;

    if (!index.isValid()) return QVariant();
    if (role != Qt::DisplayRole) return QVariant();

    QObject *obj = static_cast<QObject *>(index.internalPointer());
    
    PaneStack *paneStack = dynamic_cast<PaneStack *>(obj);
    if (paneStack) {
	std::cerr << "node is pane stack" << std::endl;
	return QVariant("Pane stack");
    }

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	// need index of pane in pane stack
	for (int i = 0; i < m_stack->getPaneCount(); ++i) {
	    if (pane == m_stack->getPane(i)) {
		std::cerr << "node is pane " << i << std::endl;
		return QVariant(QString("Pane %1").arg(i + 1));
	    }
	}
	return QVariant();
    }

    Layer *layer = dynamic_cast<Layer *>(obj);
    if (layer) {
	std::cerr << "node is layer" << std::endl;
	return QVariant(QString("%1").arg(layer->objectName()));
    }

    return QVariant();
}

/*
Qt::ItemFlags
LayerTreeModel::flags(const QModelIndex &index) const
{
}

QVariant
LayerTreeModel::headerData(const QModelIndex &index,
			   Qt::Orientation orientation,
			   int role) const
{
}
*/

QModelIndex
LayerTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    std::cerr << "LayerTreeModel::index(" << row << ", " << column << ", "
	      << &parent << ")" << std::endl;

    if (!parent.isValid()) {
	// this is the pane stack
	std::cerr << "parent invalid, returning pane stack as root" << std::endl;
	return createIndex(row, column, m_stack);
    }

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    PaneStack *paneStack = dynamic_cast<PaneStack *>(obj);
    if (paneStack) {
	if (paneStack == m_stack && row < m_stack->getPaneCount()) {
	    std::cerr << "parent is pane stack, returning a pane" << std::endl;
	    return createIndex(row, column, m_stack->getPane(row));
	}
	std::cerr << "parent is wrong pane stack, returning nothing" << std::endl;
	return QModelIndex();
    }

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	if (row < pane->getLayerCount()) {
	    std::cerr << "parent is pane, returning layer" << std::endl;
	    return createIndex(row, column, pane->getLayer(row));
	}
    }

    std::cerr << "unknown parent, returning nothing" << std::endl;
    return QModelIndex();
}

QModelIndex
LayerTreeModel::parent(const QModelIndex &index) const
{
    std::cerr << "LayerTreeModel::parent(" << &index << ")" << std::endl;

    QObject *obj = static_cast<QObject *>(index.internalPointer());
    
    PaneStack *paneStack = dynamic_cast<PaneStack *>(obj);
    if (paneStack) {
	std::cerr << "node is pane stack, returning no parent" << std::endl;
	return QModelIndex();
    }

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	std::cerr << "node is pane, returning pane stack as parent" << std::endl;
	return createIndex(0, 0, m_stack);
    }

    Layer *layer = dynamic_cast<Layer *>(obj);
    if (layer) {
	const View *view = layer->getView();
	Pane *pane = const_cast<Pane *>(dynamic_cast<const Pane *>(view));
	if (pane) {
	    // need index of pane in pane stack
	    for (int i = 0; i < m_stack->getPaneCount(); ++i) {
		if (pane == m_stack->getPane(i)) {
		    std::cerr << "node is layer, returning pane " << i << " as parent" << std::endl;
		    return createIndex(i, 0, pane);
		}
	    }
	}
	std::cerr << "node is layer, but no parent found" << std::endl;
	return QModelIndex();
    }

	

    std::cerr << "unknown node" << std::endl;
    return QModelIndex();
}

int
LayerTreeModel::rowCount(const QModelIndex &parent) const
{
    std::cerr << "LayerTreeModel::rowCount(" << &parent << ")" << std::endl;

    if (!parent.isValid()) {
	std::cerr << "parent invalid, returning 1 for the pane stack" << std::endl;
	return 1; // the pane stack
    }

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    PaneStack *paneStack = dynamic_cast<PaneStack *>(obj);
    if (paneStack) {
	if (paneStack == m_stack) {
	    std::cerr << "parent is pane stack, returning "
		      << m_stack->getPaneCount() << " panes" << std::endl;
	    return m_stack->getPaneCount();
	} else {
	    return 0;
	}
    }
 
    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	std::cerr << "parent is pane, returning "
		  << pane->getLayerCount() << " layers" << std::endl;
	return pane->getLayerCount();
    }

    std::cerr << "parent unknown, returning 0" << std::endl;
    return 0;
}

int
LayerTreeModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return 1;

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	return 1; // 2; // layer and model
    }

    return 1;
}

