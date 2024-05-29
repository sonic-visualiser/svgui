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
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QApplication>
#include <QScreen>

namespace sv {

KeyReference::KeyReference() :
    m_text(nullptr),
    m_dialog(nullptr)
{
}

KeyReference::~KeyReference()
{
    delete m_dialog;
}

void
KeyReference::setCategory(QString category)
{
    if (m_map.find(category) == m_map.end()) {
        m_categoryOrder.push_back(category);
        m_map[category] = KeyList();
    }
    m_currentCategory = category;
}

void
KeyReference::registerShortcut(QAction *action)
{
    QString name = action->text();
    name.replace(tr("&"), "");
    registerShortcut(action, name);
}

void
KeyReference::registerShortcut(QAction *action, QString name)
{
    auto shortcuts = action->shortcuts();
    int n = shortcuts.size();
    if (n == 0) return;
    registerShortcut(name, shortcuts[0], action->statusTip());
    for (int i = 1; i < n; ++i) {
        registerAlternativeShortcut(name, shortcuts[i]);
    }
}

void
KeyReference::registerShortcut(QString name, QKeySequence shortcut, QString tip)
{
    QString stext = shortcut.toString(QKeySequence::NativeText);
    registerShortcutVerbatim(name, stext, tip);
}

void
KeyReference::registerShortcutVerbatim(QString name, QString shortcut, QString tip)
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
KeyReference::registerAlternativeShortcut(QAction *action, QKeySequence shortcut)
{
    QString name = action->text();
    name.replace(tr("&"), "");
    registerAlternativeShortcut(name, shortcut);
}

void
KeyReference::registerAlternativeShortcutVerbatim(QAction *action, QString alternative)
{
    QString name = action->text();
    name.replace(tr("&"), "");
    registerAlternativeShortcutVerbatim(name, alternative);
}

void
KeyReference::registerAlternativeShortcut(QString name, QKeySequence shortcut)
{
    QString stext = shortcut.toString(QKeySequence::NativeText);
    registerAlternativeShortcutVerbatim(name, stext);
}

void
KeyReference::registerAlternativeShortcutVerbatim(QString name, QString alternative)
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
KeyReference::makeMacMouseReplacements(QString &mouseAction)
{
#ifdef Q_OS_MAC
    mouseAction.replace(tr("Ctrl+"), QChar(0x2318)); // Cmd
    mouseAction.replace(tr("Shift+"), QChar(0x21E7)); // Shift
    mouseAction.replace(tr("Alt+"), QChar(0x2325)); // Option
    mouseAction.replace(tr("Right"), QChar(0x2303)); // Ctrl
    mouseAction.replace(tr("Wheel"), tr("Scroll"));
#endif
}

void
KeyReference::registerMouseAction(QString name, QString mouseAction, QString tip)
{
    makeMacMouseReplacements(mouseAction);
    registerShortcutVerbatim(name, mouseAction, tip);
}

void
KeyReference::registerAlternativeMouseAction(QString name, QString mouseAction)
{
    makeMacMouseReplacements(mouseAction);
    registerAlternativeShortcutVerbatim(name, mouseAction);
}

void
KeyReference::show()
{
    if (m_dialog) {
        m_dialog->show();
        m_dialog->raise();
        return;
    }

    QString text;
    
    QColor bgcolor = QApplication::palette().window().color();
    bool darkbg = (bgcolor.red() + bgcolor.green() + bgcolor.blue() < 384);

    text += QString("<center><table bgcolor=\"%1\">")
        .arg(darkbg ? "#121212" : "#e8e8e8");
        
    for (CategoryList::iterator i = m_categoryOrder.begin();
         i != m_categoryOrder.end(); ++i) {

        QString category = *i;
        KeyList &list = m_map[category];

        text += QString("<tr><td bgcolor=\"%1\" colspan=3 align=\"center\"><br><b>%2</b><br></td></tr>\n").arg(darkbg ? "#303030" : "#d0d0d0").arg(category);

        for (KeyList::iterator j = list.begin(); j != list.end(); ++j) {

            QString actionName = j->actionName;

            QString shortcut = j->shortcut;
            shortcut.replace(" ", "&nbsp;");

            QString tip = j->tip;
            if (tip != "") tip = QString("<i>%1</i>").arg(tip);

            QString altdesc;
            if (!j->alternatives.empty()) {
                for (std::vector<QString>::iterator k = j->alternatives.begin();
                     k != j->alternatives.end(); ++k) {
                    QString alt = *k;
                    alt.replace(" ", "&nbsp;");
                    if (k != j->alternatives.begin()) {
                        altdesc += " ";
                    }
                    altdesc += tr("<i>or</i>&nbsp;<b>%1</b>").arg(alt);
                }
                altdesc = tr("</b>&nbsp;(%1)<b>").arg(altdesc);
            }

            text += QString("<tr><td width=\"12%\">&nbsp;<b>%1%2</b></td><td>&nbsp;%3</td><td>%4</td></tr>\n")
                .arg(shortcut).arg(altdesc).arg(actionName).arg(tip);
        }
    }

    text += "</table></center>\n";

    m_text = new QTextEdit;
    m_text->setHtml(text);
    m_text->setReadOnly(true);

    m_dialog = new QDialog;
    m_dialog->setWindowTitle(tr("%1: Key and Mouse Reference")
                             .arg(QApplication::applicationName()));

    QVBoxLayout *layout = new QVBoxLayout;
    m_dialog->setLayout(layout);
    layout->addWidget(m_text);

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, SIGNAL(clicked(QAbstractButton *)), this, SLOT(dialogButtonClicked(QAbstractButton *)));
    layout->addWidget(bb);

    m_dialog->show();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect available = screen->availableGeometry();

    int width = available.width() * 3 / 5;
    int height = available.height() * 2 / 3;
    if (height < 450) {
        if (available.height() > 500) height = 450;
    }
    if (width < 600) {
        if (available.width() > 650) width = 600;
    }

    m_dialog->resize(width, height);
    m_dialog->raise();
}

void
KeyReference::dialogButtonClicked(QAbstractButton *)
{
    // only button is Close
    m_dialog->hide();
}

void
KeyReference::hide()
{
    if (m_dialog) {
        m_dialog->hide();
    }
}
} // end namespace sv

