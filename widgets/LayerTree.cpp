
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

#include "base/PlayParameters.h"
#include "view/Pane.h"
#include "layer/Layer.h"
#include "data/model/Model.h"

#include <QIcon>
#include <iostream>


LayerTreeModel::LayerTreeModel(PaneStack *stack, QObject *parent) :
    QAbstractItemModel(parent),
    m_stack(stack)
{
    connect(stack, SIGNAL(paneAdded()), this, SIGNAL(layoutChanged()));
    connect(stack, SIGNAL(paneDeleted()), this, SIGNAL(layoutChanged()));

    for (int i = 0; i < stack->getPaneCount(); ++i) {
        Pane *pane = stack->getPane(i);
        if (!pane) continue;
        connect(pane, SIGNAL(propertyContainerAdded(PropertyContainer *)),
                this, SLOT(propertyContainerAdded(PropertyContainer *)));
        connect(pane, SIGNAL(propertyContainerRemoved(PropertyContainer *)),
                this, SLOT(propertyContainerRemoved(PropertyContainer *)));
        connect(pane, SIGNAL(propertyContainerSelected(PropertyContainer *)),
                this, SLOT(propertyContainerSelected(PropertyContainer *)));
        connect(pane, SIGNAL(propertyContainerPropertyChanged(PropertyContainer *)),
                this, SLOT(propertyContainerPropertyChanged(PropertyContainer *)));
        connect(pane, SIGNAL(propertyContainerNameChanged(PropertyContainer *)),
                this, SLOT(propertyContainerPropertyChanged(PropertyContainer *)));
        for (int j = 0; j < pane->getLayerCount(); ++j) {
            Layer *layer = pane->getLayer(j);
            if (!layer) continue;
            PlayParameters *params = layer->getPlayParameters();
            if (!params) continue;
            connect(params, SIGNAL(playAudibleChanged(bool)),
                    this, SLOT(playParametersAudibilityChanged(bool)));
        }
    }
}

LayerTreeModel::~LayerTreeModel()
{
}

void
LayerTreeModel::propertyContainerAdded(PropertyContainer *)
{
    emit layoutChanged();
}

void
LayerTreeModel::propertyContainerRemoved(PropertyContainer *)
{
    emit layoutChanged();
}

void
LayerTreeModel::propertyContainerSelected(PropertyContainer *)
{
    emit layoutChanged();
}

void
LayerTreeModel::propertyContainerPropertyChanged(PropertyContainer *pc)
{
    for (int i = 0; i < m_stack->getPaneCount(); ++i) {
        Pane *pane = m_stack->getPane(i);
        if (!pane) continue;
        for (int j = 0; j < pane->getLayerCount(); ++j) {
            if (pane->getLayer(j) == pc) {
                emit dataChanged(createIndex(pane->getLayerCount() - j - 1,
                                             0, pane),
                                 createIndex(pane->getLayerCount() - j - 1,
                                             3, pane));
            }
        }
    }
}

void
LayerTreeModel::playParametersAudibilityChanged(bool a)
{
    PlayParameters *params = dynamic_cast<PlayParameters *>(sender());
    if (!params) return;

    std::cerr << "LayerTreeModel::playParametersAudibilityChanged("
              << params << "," << a << ")" << std::endl;

    for (int i = 0; i < m_stack->getPaneCount(); ++i) {
        Pane *pane = m_stack->getPane(i);
        if (!pane) continue;
        for (int j = 0; j < pane->getLayerCount(); ++j) {
            Layer *layer = pane->getLayer(j);
            if (!layer) continue;
            if (layer->getPlayParameters() == params) {
                std::cerr << "LayerTreeModel::playParametersAudibilityChanged("
                          << params << "," << a << "): row " << pane->getLayerCount() - j - 1 << ", col " << 2 << std::endl;

                emit dataChanged(createIndex(pane->getLayerCount() - j - 1,
                                             2, pane),
                                 createIndex(pane->getLayerCount() - j - 1,
                                             2, pane));
            }
        }
    }
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
        Layer *layer = pane->getLayer(pane->getLayerCount() - row - 1);
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
                if (role == Qt::CheckStateRole) {
                    return QVariant(layer->isLayerDormant(pane) ?
                                    Qt::Unchecked : Qt::Checked);
                } else if (role == Qt::TextAlignmentRole) {
                    return QVariant(Qt::AlignHCenter);
                }
            } else if (col == 2) {
                if (role == Qt::CheckStateRole) {
                    PlayParameters *params = layer->getPlayParameters();
                    if (params) return QVariant(params->isPlayMuted() ?
                                                Qt::Unchecked : Qt::Checked);
                    else return QVariant();
                } else if (role == Qt::TextAlignmentRole) {
                    return QVariant(Qt::AlignHCenter);
                }
            } else if (col == 3) {
                Model *model = layer->getModel();
                if (model && role == Qt::DisplayRole) {
                    return QVariant(model->objectName());
                }
            }
        }
    }

    return QVariant();
}

bool
LayerTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) return false;

    QObject *obj = static_cast<QObject *>(index.internalPointer());
    int row = index.row(), col = index.column();

    Pane *pane = dynamic_cast<Pane *>(obj);
    if (!pane || pane->getLayerCount() <= row) return false;

    Layer *layer = pane->getLayer(pane->getLayerCount() - row - 1);
    if (!layer) return false;

    if (col == 1) {
        if (role == Qt::CheckStateRole) {
            layer->showLayer(pane, value.toInt() == Qt::Checked);
            emit dataChanged(index, index);
            return true;
        }
    } else if (col == 2) {
        if (role == Qt::CheckStateRole) {
            PlayParameters *params = layer->getPlayParameters();
            if (params) {
                params->setPlayMuted(value.toInt() == Qt::Unchecked);
                emit dataChanged(index, index);
                return true;
            }
        }
    }

    return false;
}

Qt::ItemFlags
LayerTreeModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = Qt::ItemIsEnabled;
    if (!index.isValid()) return flags;

    if (index.column() == 1 || index.column() == 2) {
        flags |= Qt::ItemIsUserCheckable;
    } else if (index.column() == 0) {
        flags |= Qt::ItemIsSelectable;
    }

    return flags;
}

QVariant
LayerTreeModel::headerData(int section,
			   Qt::Orientation orientation,
			   int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
	if (section == 0) return QVariant(tr("Layer"));
        else if (section == 1) return QVariant(tr("Shown"));
        else if (section == 2) return QVariant(tr("Played"));
	else if (section == 3) return QVariant(tr("Model"));
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
    if (!parent.isValid()) return 4;

    QObject *obj = static_cast<QObject *>(parent.internalPointer());
    if (obj == m_stack) return 4; // row for a layer

    return 1;
}

