/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SubdividingMenu.h"

#include <iostream>

using std::set;
using std::map;

SubdividingMenu::SubdividingMenu(QWidget *parent) :
    QMenu(parent)
{
}

SubdividingMenu::SubdividingMenu(const QString &title, QWidget *parent) :
    QMenu(title, parent)
{
}

SubdividingMenu::~SubdividingMenu()
{
}

void
SubdividingMenu::setEntries(const std::set<QString> &entries)
{
    size_t total = entries.size();
    size_t chunk = 14;
        
    if (total < (chunk * 3) / 2) return;

    size_t count = 0;
    QMenu *chunkMenu = new QMenu();

    QString firstNameInChunk;
    QChar firstInitialInChunk;
    bool discriminateStartInitial = false;

    for (set<QString>::const_iterator j = entries.begin();
         j != entries.end();
         ++j) {

        std::cerr << "SubdividingMenu::setEntries: j -> " << j->toStdString() << std::endl;

        m_nameToChunkMenuMap[*j] = chunkMenu;

        set<QString>::iterator k = j;
        ++k;

        QChar initial = (*j)[0];

        if (count == 0) {
            firstNameInChunk = *j;
            firstInitialInChunk = initial;
        }

        bool lastInChunk = (k == entries.end() ||
                            (count >= chunk-1 &&
                             (count == (5*chunk) / 2 ||
                              (*k)[0] != initial)));

        ++count;

        if (lastInChunk) {

            bool discriminateEndInitial = (k != entries.end() &&
                                           (*k)[0] == initial);

            bool initialsEqual = (firstInitialInChunk == initial);

            QString from = QString("%1").arg(firstInitialInChunk);
            if (discriminateStartInitial ||
                (discriminateEndInitial && initialsEqual)) {
                from = firstNameInChunk.left(3);
            }

            QString to = QString("%1").arg(initial);
            if (discriminateEndInitial ||
                (discriminateStartInitial && initialsEqual)) {
                to = j->left(3);
            }

            QString menuText;
            
            if (from == to) menuText = from;
            else menuText = tr("%1 - %2").arg(from).arg(to);
            
            discriminateStartInitial = discriminateEndInitial;

            chunkMenu->setTitle(menuText);
                
            QMenu::addMenu(chunkMenu);
            
            chunkMenu = new QMenu();
            
            count = 0;
        }
    }
    
    if (count == 0) delete chunkMenu;
}

void
SubdividingMenu::addAction(QAction *action)
{
    QString name = action->text();

    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        QMenu::addAction(action);
        return;
    }

    std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    m_nameToChunkMenuMap[name]->addAction(action);
}

QAction *
SubdividingMenu::addAction(const QString &name)
{
    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        return QMenu::addAction(name);
    }

    std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    return m_nameToChunkMenuMap[name]->addAction(name);
}

void
SubdividingMenu::addAction(const QString &name, QAction *action)
{
    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        QMenu::addAction(action);
        return;
    }

    std::cerr << "SubdividingMenu::addAction(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    m_nameToChunkMenuMap[name]->addAction(action);
}

void
SubdividingMenu::addMenu(QMenu *menu)
{
    QString name = menu->title();

    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        QMenu::addMenu(menu);
        return;
    }

    std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    m_nameToChunkMenuMap[name]->addMenu(menu);
}

QMenu *
SubdividingMenu::addMenu(const QString &name)
{
    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        return QMenu::addMenu(name);
    }

    std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    return m_nameToChunkMenuMap[name]->addMenu(name);
}

void
SubdividingMenu::addMenu(const QString &name, QMenu *menu)
{
    if (m_nameToChunkMenuMap.find(name) == m_nameToChunkMenuMap.end()) {
        std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): not found in name-to-chunk map, adding to main menu" << std::endl;
        QMenu::addMenu(menu);
        return;
    }

    std::cerr << "SubdividingMenu::addMenu(" << name.toStdString() << "): found in name-to-chunk map for menu " << m_nameToChunkMenuMap[name]->title().toStdString() << std::endl;
    m_nameToChunkMenuMap[name]->addMenu(menu);
}

