/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "ModelDataTableDialog.h"

#include "data/model/ModelDataTableModel.h"
#include "data/model/TabularModel.h"
#include "data/model/Model.h"

#include "CommandHistory.h"
#include "IconLoader.h"

#include <QTableView>
#include <QGridLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopWidget>
#include <QAction>
#include <QToolBar>

#include <iostream>

ModelDataTableDialog::ModelDataTableDialog(TabularModel *model, QString title, QWidget *parent) :
    QMainWindow(parent),
    m_currentRow(0),
    m_trackPlayback(false)
{
    setWindowTitle(tr("Data Editor"));

    QToolBar *toolbar = addToolBar(tr("Toolbar"));

    IconLoader il;

    QAction *action = new QAction(il.load("playfollow"), tr("Track Playback"), this);
    action->setStatusTip(tr("Toggle tracking of playback position"));
    action->setCheckable(true);
    connect(action, SIGNAL(triggered()), this, SLOT(togglePlayTracking()));
    toolbar->addAction(action);

    CommandHistory::getInstance()->registerToolbar(toolbar);

    action = new QAction(il.load("datainsert"), tr("Insert New Item"), this);
    action->setShortcut(tr("Insert"));
    action->setStatusTip(tr("Insert a new item"));
    connect(action, SIGNAL(triggered()), this, SLOT(insertRow()));
    toolbar->addAction(action);

    action = new QAction(il.load("datadelete"), tr("Delete Selected Items"), this);
    action->setShortcut(tr("Delete"));
    action->setStatusTip(tr("Delete the selected item or items"));
    connect(action, SIGNAL(triggered()), this, SLOT(deleteRows()));
    toolbar->addAction(action);

    action = new QAction(il.load("dataedit"), tr("Edit Selected Item"), this);
    action->setShortcut(tr("Edit"));
    action->setStatusTip(tr("Edit the selected item"));
    connect(action, SIGNAL(triggered()), this, SLOT(editRow()));
    toolbar->addAction(action);

    QFrame *mainFrame = new QFrame;
    setCentralWidget(mainFrame);

    QGridLayout *grid = new QGridLayout;
    mainFrame->setLayout(grid);
    
    QGroupBox *box = new QGroupBox;
    if (title != "") {
        box->setTitle(title);
    } else {
        box->setTitle(tr("Data in Layer"));
    }
    grid->addWidget(box, 0, 0);
    grid->setRowStretch(0, 15);

    QGridLayout *subgrid = new QGridLayout;
    box->setLayout(subgrid);

    subgrid->setSpacing(0);
    subgrid->setMargin(5);

    m_tableView = new QTableView;
    subgrid->addWidget(m_tableView);

//    m_tableView->verticalHeader()->hide();
//    m_tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    m_tableView->setSortingEnabled(true);
    m_tableView->sortByColumn(0, Qt::AscendingOrder);

    m_table = new ModelDataTableModel(model);
    m_tableView->setModel(m_table);

    connect(m_tableView, SIGNAL(clicked(const QModelIndex &)),
            this, SLOT(viewClicked(const QModelIndex &)));
    connect(m_tableView, SIGNAL(pressed(const QModelIndex &)),
            this, SLOT(viewPressed(const QModelIndex &)));
    connect(m_tableView->selectionModel(),
            SIGNAL(currentChanged(const QModelIndex &, const QModelIndex &)),
            this,
            SLOT(currentChanged(const QModelIndex &, const QModelIndex &)));
    connect(m_table, SIGNAL(addCommand(Command *)),
            this, SLOT(addCommand(Command *)));
    connect(m_table, SIGNAL(currentChanged(const QModelIndex &)),
            this, SLOT(currentChangedThroughResort(const QModelIndex &)));

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, SIGNAL(rejected()), this, SLOT(close()));
    grid->addWidget(bb, 2, 0);
    grid->setRowStretch(2, 0);
    
    QDesktopWidget *desktop = QApplication::desktop();
    QRect available = desktop->availableGeometry();

    int width = available.width() / 3;
    int height = available.height() / 2;
    if (height < 370) {
        if (available.height() > 500) height = 370;
    }
    if (width < 500) {
        if (available.width() > 650) width = 500;
    }

    resize(width, height);
}

ModelDataTableDialog::~ModelDataTableDialog()
{
    delete m_table;
}

void
ModelDataTableDialog::userScrolledToFrame(unsigned long frame)
{
    QModelIndex index = m_table->getModelIndexForFrame(frame);
    makeCurrent(index.row());
}

void
ModelDataTableDialog::playbackScrolledToFrame(unsigned long frame)
{
    if (m_trackPlayback) {
        QModelIndex index = m_table->getModelIndexForFrame(frame);
        makeCurrent(index.row());
    }
}

void
ModelDataTableDialog::makeCurrent(int row)
{
    int rh = m_tableView->height() / m_tableView->rowHeight(0);
    int topRow = row - rh/2;
    if (topRow < 0) topRow = 0;
    //!!! should not do any of this if an item in the given row is
    //already current; should not scroll if the current row is already
    //visible
    std::cerr << "rh = " << rh << ", row = " << row << ", scrolling to "
              << topRow << std::endl;
    m_tableView->scrollTo
        (m_table->getModelIndexForRow(topRow));
    m_tableView->selectionModel()->setCurrentIndex
        (m_table->getModelIndexForRow(row),
         QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void
ModelDataTableDialog::viewClicked(const QModelIndex &index)
{
    std::cerr << "ModelDataTableDialog::viewClicked: " << index.row() << ", " << index.column() << std::endl;
    emit scrollToFrame(m_table->getFrameForModelIndex(index));
}

void
ModelDataTableDialog::viewPressed(const QModelIndex &index)
{
    std::cerr << "ModelDataTableDialog::viewPressed: " << index.row() << ", " << index.column() << std::endl;
}

void
ModelDataTableDialog::currentChanged(const QModelIndex &current,
                                     const QModelIndex &previous)
{
    std::cerr << "ModelDataTableDialog::currentChanged: from "
              << previous.row() << ", " << previous.column()
              << " to " << current.row() << ", " << current.column() 
              << std::endl;
    m_currentRow = current.row();
    m_table->setCurrentRow(m_currentRow);
}

void
ModelDataTableDialog::insertRow()
{
    m_table->insertRow(m_currentRow);
}

void
ModelDataTableDialog::deleteRows()
{
    // not efficient
    while (m_tableView->selectionModel()->hasSelection()) {
        m_table->removeRow
            (m_tableView->selectionModel()->selection().indexes().begin()->row());
    }
}

void
ModelDataTableDialog::editRow()
{
}

void
ModelDataTableDialog::addCommand(Command *command)
{
    CommandHistory::getInstance()->addCommand(command, false, true);
}

void
ModelDataTableDialog::togglePlayTracking()
{
    m_trackPlayback = !m_trackPlayback;
}

void
ModelDataTableDialog::currentChangedThroughResort(const QModelIndex &index)
{
    std::cerr << "ModelDataTableDialog::currentChangedThroughResort: row = " << index.row() << std::endl;
//  m_tableView->scrollTo(index);
    makeCurrent(index.row());
}


    
