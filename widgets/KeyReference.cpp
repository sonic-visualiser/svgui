/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "KeyReference.h"

#include <QAction>
#include <QTextEdit>

KeyReference::KeyReference() :
    m_text(0)
{
}

KeyReference::~KeyReference()
{
    delete m_text;
}

void
KeyReference::setCategory(QString category)
{
    if (m_map.find(category) == m_map.end()) {
        m_categoryOrder.push_back(category);
    }
    m_currentCategory = category;
    m_map[category] = KeyList();
}

void
KeyReference::registerShortcut(QAction *action, QString overrideName)
{
    QString name = action->text();
    if (overrideName != "") name = overrideName;

    QString shortcut = action->shortcut();
    QString tip = action->statusTip();

    registerShortcut(name, shortcut, tip);
}

void
KeyReference::registerShortcut(QString name, QString shortcut, QString tip)
{
    KeyList &list = m_map[m_currentCategory];

    for (KeyList::iterator i = list.begin(); i != list.end(); ++i) {
        if (i->actionName == name) {
            i->shortcut = shortcut;
            i->tip = tip;
            i->alternatives.clear();
            return;
        }
    }

    KeyDetails details;
    details.actionName = name;
    details.shortcut = shortcut;
    details.tip = tip;

    list.push_back(details);
}

void
KeyReference::registerAlternativeShortcut(QAction *action, QString alternative)
{
    QString name = action->text();
    registerAlternativeShortcut(name, alternative);
}

void
KeyReference::registerAlternativeShortcut(QString name, QString alternative)
{
    KeyList &list = m_map[m_currentCategory];

    for (KeyList::iterator i = list.begin(); i != list.end(); ++i) {
        if (i->actionName == name) {
            i->alternatives.push_back(alternative);
            return;
        }
    }
}

void
KeyReference::show()
{
    if (m_text) {
        m_text->show();
        m_text->raise();
        return;
    }

    QString text;

    text += "<center><table bgcolor=\"#e8e8e8\">";
        
    for (CategoryList::iterator i = m_categoryOrder.begin();
         i != m_categoryOrder.end(); ++i) {

        QString category = *i;
        KeyList &list = m_map[category];

        text += QString("<tr><td bgcolor=\"#d0d0d0\" colspan=3><br>&nbsp;<b>%1</b><br></td></tr>\n").arg(category);

        for (KeyList::iterator j = list.begin(); j != list.end(); ++j) {

            QString actionName = j->actionName;
            actionName.replace(tr("&"), "");

            QString tip = j->tip;
            if (tip != "") tip = QString("<i>%1</i>").arg(tip);

            QString altdesc;
            if (!j->alternatives.empty()) {
                for (std::vector<QString>::iterator k = j->alternatives.begin();
                     k != j->alternatives.end(); ++k) {
                    altdesc += tr("<i>or</i>&nbsp;<b>%1</b>").arg(*k);
                }
                altdesc = tr("</b>&nbsp;(%1)<b>").arg(altdesc);
            }

            text += QString("<tr><td>&nbsp;<b>%1%2</b></td><td>&nbsp;%3</td><td>%4</td></tr>\n")
                .arg(j->shortcut).arg(altdesc).arg(actionName).arg(tip);
        }
    }

    text += "</table></center>\n";

    m_text = new QTextEdit;
    m_text->setHtml(text);
    m_text->setReadOnly(true);
    m_text->setObjectName(tr("Key Reference"));
    m_text->show();
    m_text->resize(600, 450);
    m_text->raise();
}

