
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

#include "LayerTree.h"
#include "PaneStack.h"

#include "widgets/Pane.h"
#include "base/Layer.h"
#include "base/Model.h"

#include <iostream>


class ViewObjectAssoc : public QObject
{
public:
    ViewObjectAssoc(QObject *parent, View *v, QObject *o) :
	QObject(parent), view(v), object(o) {
	++extantCount;
    }

    virtual ~ViewObjectAssoc() {
	std::cerr << "~ViewObjectAssoc (now " << --extantCount << " extant)"
		  << std::endl;
    }

    View *view;
    QObject *object;

    static int extantCount;
};

int ViewObjectAssoc::extantCount = 0;


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
    if (!index.isValid()) return QVariant();
    if (role != Qt::DisplayRole) return QVariant();

    std::cerr << "LayerTreeModel::data(" << &index << ", role " << role << ")" << std::endl;

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

    ViewObjectAssoc *assoc = dynamic_cast<ViewObjectAssoc *>(obj);
    if (assoc) {
	std::cerr << "node is assoc" << std::endl;
	Layer *layer = dynamic_cast<Layer *>(assoc->object);
	if (layer) {
	    std::cerr << "with layer" << std::endl;
	    return QVariant(layer->objectName());
	}
	Model *model = dynamic_cast<Model *>(assoc->object);
	if (model) {
	    std::cerr << "with model" << std::endl;
	    return QVariant(model->objectName());
	}
    }

    return QVariant();
}

Qt::ItemFlags
LayerTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::ItemIsEnabled;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant
LayerTreeModel::headerData(int section,
			   Qt::Orientation orientation,
			   int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
	if (section == 0) return QVariant(tr("Layer"));
	else if (section == 1) return QVariant(tr("Model"));
    }

    return QVariant();
}

QModelIndex
LayerTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    std::cerr << "LayerTreeModel::index(" << row << ", " << column << ", "
	      << &parent << ")" << std::endl;

    if (!parent.isValid()) {
	// this is the pane stack
	std::cerr << "parent invalid, returning pane stack as root" << std::endl;
	if (column > 0) return QModelIndex();
	return createIndex(row, column, m_stack);
    }

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    PaneStack *paneStack = dynamic_cast<PaneStack *>(obj);
    if (paneStack) {
	if (column > 0) return QModelIndex();
	if (paneStack == m_stack && row < m_stack->getPaneCount()) {
	    std::cerr << "parent is pane stack, returning a pane" << std::endl;
	    return createIndex(row, column, m_stack->getPane(row));
	}
	std::cerr << "parent is wrong pane stack, returning nothing" << std::endl;
	return QModelIndex();
    }

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	std::cerr << "parent is pane" << std::endl;
	if (row < pane->getLayerCount()) {
	    Layer *layer = pane->getLayer(row);
	    if (column == 0) {
		std::cerr << "parent is pane, returning layer" << std::endl;
		ViewObjectAssoc *assoc = new ViewObjectAssoc
		    (const_cast<LayerTreeModel *>(this), pane, layer);
		return createIndex(row, column, assoc);
	    } else {
		std::cerr << "parent is pane, column != 0, returning model" << std::endl;
		ViewObjectAssoc *assoc = new ViewObjectAssoc
		    (const_cast<LayerTreeModel *>(this), pane, layer->getModel());
		return createIndex(row, column, assoc);
	    }		
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

    ViewObjectAssoc *assoc = dynamic_cast<ViewObjectAssoc *>(obj);
    if (assoc) {
	View *view = assoc->view;
	Pane *pane = dynamic_cast<Pane *>(view);
	if (pane) {
	    // need index of pane in pane stack
	    for (int i = 0; i < m_stack->getPaneCount(); ++i) {
		if (pane == m_stack->getPane(i)) {
		    std::cerr << "node is assoc, returning pane " << i << " as parent" << std::endl;
		    return createIndex(i, 0, pane);
		}
	    }
	}
	std::cerr << "node is assoc, but no parent found" << std::endl;
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
    if (!parent.isValid()) {
	std::cerr << "LayerTreeModel::columnCount: parent invalid, returning 2" << std::endl;
	return 2;
    }

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
	std::cerr << "LayerTreeModel::columnCount: pane, returning 2" << std::endl;
	return 2; // layer and model
    }

    std::cerr << "LayerTreeModel::columnCount: returning 1" << std::endl;

    return 1;
}

