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

#ifndef SV_TIP_DIALOG_H
#define SV_TIP_DIALOG_H

#include <QDialog>
#include <QString>
#include <QXmlDefaultHandler>

#include <vector>

#include "base/Debug.h"

class QLabel;
class QXmlInputSource;

namespace sv {

class TipDialog : public QDialog
{
    Q_OBJECT

public:
    TipDialog(QWidget *parent = 0);
    virtual ~TipDialog();

    bool isOK() { return !m_tips.empty(); }

protected slots:
    void previous();
    void next();

protected:
    int m_tipNumber;
    QLabel *m_label;
    QString m_caption;

    std::vector<QString> m_tips;

    void readTips();
    void showTip();

    class TipFileParser : public QXmlDefaultHandler
    {
    public:
        TipFileParser(TipDialog *dialog);
        virtual ~TipFileParser();
        
        void parse(QXmlInputSource &source);

        bool startElement(const QString &namespaceURI,
                                  const QString &localName,
                                  const QString &qName,
                                  const QXmlAttributes& atts) override;
        
        bool characters(const QString &) override;
        
        bool endElement(const QString &namespaceURI,
                                const QString &localName,
                                const QString &qName) override;
        
        bool error(const QXmlParseException &exception) override;
        bool fatalError(const QXmlParseException &exception) override;

    protected:
        TipDialog *m_dialog;

        bool m_inTip;
        bool m_inText;
        bool m_inHtml;
    };
};

} // end namespace sv

#endif
