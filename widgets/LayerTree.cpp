
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
#include "view/PaneStack.h"

#include "view/Pane.h"
#include "layer/Layer.h"
#include "data/model/Model.h"

#include <iostream>


LayerTreeModel::LayerTreeModel(PaneStack *stack, QObject *parent) :
    QAbstractItemModel(parent),
    m_stack(stack)
{
    connect(stack, SIGNAL(paneAdded()), this, SIGNAL(layoutChanged()));
    connect(stack, SIGNAL(paneDeleted()), this, SIGNAL(layoutChanged()));
}

LayerTreeModel::~LayerTreeModel()
{
}

QVariant
LayerTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();

    QObject *obj = static_cast<QObject *>(index.internalPointer());
    int row = index.row(), col = index.column();

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (!pane) {
        if (col == 0 && row < m_stack->getPaneCount()) {
            switch (role) {
            case Qt::DisplayRole:
                return QVariant(QString("Pane %1").arg(row + 1));
            case Qt::DecorationRole:
                return QVariant(QIcon(QString(":/icons/pane.png")));
            default: break;
            }
        }
    }

    if (pane && pane->getLayerCount() > row) {
        Layer *layer = pane->getLayer(row);
        if (layer) {
            if (col == 0) {
                switch (role) {
                case Qt::DisplayRole:
                    return QVariant(layer->objectName());
                case Qt::DecorationRole:
                    return QVariant
                        (QIcon(QString(":/icons/%1.png")
                               .arg(layer->getPropertyContainerIconName())));
                default: break;
                }
            } else if (col == 1) {
                Model *model = layer->getModel();
                if (model && role == Qt::DisplayRole) {
                    return QVariant(model->objectName());
                }
            }
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
    // cell for a pane contains row, column, pane stack
    // -> its parent is the invalid cell

    // cell for a layer contains row, column, pane
    // -> its parent is row, column, pane stack (which identify the pane)

    if (!parent.isValid()) {
        if (row >= m_stack->getPaneCount() || column > 0) return QModelIndex();
	return createIndex(row, column, m_stack);
    }

    QObject *obj = static_cast<QObject *>(parent.internalPointer());

    if (obj == m_stack) {
        Pane *pane = m_stack->getPane(parent.row());
        if (!pane || parent.column() > 0) return QModelIndex();
        return createIndex(row, column, pane);
    }

    return QModelIndex();
}

QModelIndex
LayerTreeModel::parent(const QModelIndex &index) const
{
    QObject *obj = static_cast<QObject *>(index.internalPointer());

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (pane) {
        int index = m_stack->getPaneIndex(pane);
        if (index >= 0) return createIndex(index, 0, m_stack);
    }

    return QModelIndex();
}

int
LayerTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return m_stack->getPaneCount();

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    
    if (obj == m_stack) {
        Pane *pane = m_stack->getPane(parent.row());
        if (!pane || parent.column() > 0) return 0;
        return pane->getLayerCount();
    }

    return 0;
}

int
LayerTreeModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return 2;

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    if (obj == m_stack) return 2;

    return 1;
}

