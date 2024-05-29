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

#ifndef SV_KEY_REFERENCE_H
#define SV_KEY_REFERENCE_H

#include <QObject>
#include <QString>
#include <vector>
#include <map>
#include <QKeySequence>

class QAction;
class QTextEdit;
class QDialog;
class QAbstractButton;

namespace sv {

class KeyReference : public QObject
{
    Q_OBJECT

public:
    KeyReference();
    virtual ~KeyReference();

    void setCategory(QString category);

    /** Register the key sequence for the given action using the text
     *  of that action as the name of the assocated operation.
     */
    void registerShortcut(QAction *);

    /** Register the key sequence for the given action using the given
     *  name as the name of the assocated operation.
     */
    void registerShortcut(QAction *, QString actionName);

    /** Register the given key sequence as the primary shortcut for
     *  the action of the given name, with the given descriptive text.
     */
    void registerShortcut(QString actionName, QKeySequence shortcut, QString tipText);
    
    /** Register the given mouse mechanism as a shortcut for the
     *  action of the given name, with the given descriptive
     *  text. This is similar to registerShortcutVerbatim in that the
     *  mouse action is used almost literally, except that modifiers
     *  such as Ctrl are remapped appropriately for the Mac in a
     *  similar manner to QKeySequence.
     */
    void registerMouseAction(QString actionName, QString mouseAction, QString tipText);
    
    /** Register the given text as the primary shortcut for the action
     *  of the given name, with the given descriptive text. The
     *  shortcut text is used verbatim and does not necessarily have
     *  to be a key sequence.
     */
    void registerShortcutVerbatim(QString actionName, QString shortcut, QString tipText);

    /** Register the given key sequence as an alternative shortcut for
     *  the given action.
     */
    void registerAlternativeShortcut(QAction *, QKeySequence alternative);

    /** Register the given text as an alternative shortcut for the
     *  given action. The text is used verbatim and does not
     *  necessarily have to be a key sequence.
     */
    void registerAlternativeShortcutVerbatim(QAction *, QString alternative);
    
    /** Register the given key sequence as an alternative shortcut for
     *  the action of the given name.
     */
    void registerAlternativeShortcut(QString actionName, QKeySequence alternative);
    
    /** Register the given mouse mechanism as an alternative shortcut
     *  for the action of the given name. This is similar to
     *  registerAlternativeShortcutVerbatim in that the mouse action
     *  is used almost literally, except that modifiers such as Ctrl
     *  are remapped appropriately for the Mac in a similar manner to
     *  QKeySequence.
     */
    void registerAlternativeMouseAction(QString actionName, QString mouseAction);

    /** Register the given text as an alternative shortcut for the
     *  action of the given name. The text is used verbatim and does
     *  not necessarily have to be a key sequence.
     */
    void registerAlternativeShortcutVerbatim(QString actionName, QString alternative);

    void show();
    void hide();

protected slots:
    void dialogButtonClicked(QAbstractButton *);

protected:
    struct KeyDetails {
        QString actionName;
        QString shortcut;
        QString tip;
        std::vector<QString> alternatives;
    };

    void makeMacMouseReplacements(QString &);
    
    typedef std::vector<KeyDetails> KeyList;
    typedef std::map<QString, KeyList> CategoryMap;
    typedef std::vector<QString> CategoryList;
    
    QString m_currentCategory;
    CategoryMap m_map;
    CategoryList m_categoryOrder;

    QTextEdit *m_text;
    QDialog *m_dialog;
};

} // end namespace sv

#endif
