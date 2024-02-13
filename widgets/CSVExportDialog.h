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

#ifndef SV_CSV_EXPORT_DIALOG_H
#define SV_CSV_EXPORT_DIALOG_H

#include <QDialog>
#include <QString>

class QComboBox;
class QCheckBox;
class QRadioButton;

namespace sv {

class CSVExportDialog : public QDialog
{
    Q_OBJECT

public:
    struct Configuration {
        Configuration() :
            layerName(""),
            fileExtension("csv"),
            isDense(false),
            haveView(false),
            haveSelection(false) { }

        /**
         * Presentation name of the layer being exported.
         */
        QString layerName;

        /**
         * Extension of file being exported into.
         */
        QString fileExtension;

        /**
         * True if the model is a dense type for which timestamps are
         * not written by default.
         */
        bool isDense;

        /**
         * True if we have a view that provides a vertical scale
         * range, so we may want to offer a choice between exporting
         * only the visible range or exporting full height. This
         * choice happens to be offered only if isDense is also true.
         */
        bool haveView;

        /**
         * True if there is a selection current that the user may want
         * to constrain export to.
         */
        bool haveSelection;
    };
    
    CSVExportDialog(Configuration config, QWidget *parent);

    /**
     * Return the column delimiter to use in the exported file. Either
     * the default for the supplied file extension, or some other
     * option chosen by the user.
     */
    QString getDelimiter() const;

    /**
     * Return true if we should include a header row at the top of the
     * exported file.
     */
    bool shouldIncludeHeader() const;

    /**
     * Return true if we should write a timestamp column. This is
     * always true for non-dense models, but is a user option for
     * dense ones.
     */
    bool shouldIncludeTimestamps() const;

    /**
     * Return true if we should use sample frames rather than seconds
     * for the timestamp column (and duration where present).
     */
    bool shouldWriteTimeInFrames() const;

    /**
     * Return true if we should constrain the vertical range to the
     * visible area only. Otherwise we should export the full vertical
     * range of the model.
     */
    bool shouldConstrainToViewHeight() const;

    /**
     * Return true if we should export the selected time range(s)
     * only. Otherwise we should export the full length of the model.
     */
    bool shouldConstrainToSelection() const;

private:
    Configuration m_config;

    QComboBox *m_separatorCombo;
    QCheckBox *m_header;
    QCheckBox *m_timestamps;
    QRadioButton *m_seconds;
    QRadioButton *m_frames;
    QRadioButton *m_selectionOnly;
    QRadioButton *m_viewOnly;

private slots:
    void timestampsToggled(bool);
};

} // end namespace sv

#endif
