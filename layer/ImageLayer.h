/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _IMAGE_LAYER_H_
#define _IMAGE_LAYER_H_

#include "Layer.h"
#include "data/model/ImageModel.h"

#include <QObject>
#include <QColor>
#include <QImage>
#include <QMutex>

#include <map>

class View;
class QPainter;
class FileSource;

class ImageLayer : public Layer
{
    Q_OBJECT

public:
    ImageLayer();
    virtual ~ImageLayer();

    void paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const override;

    QString getFeatureDescription(LayerGeometryProvider *v, QPoint &) const override;

    bool snapToFeatureFrame(LayerGeometryProvider *v, sv_frame_t &frame,
                                    int &resolution,
                                    SnapType snap) const override;

    void drawStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void drawEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void editStart(LayerGeometryProvider *v, QMouseEvent *) override;
    void editDrag(LayerGeometryProvider *v, QMouseEvent *) override;
    void editEnd(LayerGeometryProvider *v, QMouseEvent *) override;

    void moveSelection(Selection s, sv_frame_t newStartFrame) override;
    void resizeSelection(Selection s, Selection newSize) override;
    void deleteSelection(Selection s) override;

    void copy(LayerGeometryProvider *v, Selection s, Clipboard &to) override;
    bool paste(LayerGeometryProvider *v, const Clipboard &from, sv_frame_t frameOffset,
                       bool interactive) override;

    bool editOpen(LayerGeometryProvider *, QMouseEvent *) override; // on double-click

    const Model *getModel() const override { return m_model; }
    void setModel(ImageModel *model);

    PropertyList getProperties() const override;
    QString getPropertyLabel(const PropertyName &) const override;
    PropertyType getPropertyType(const PropertyName &) const override;
    int getPropertyRangeAndValue(const PropertyName &,
                                         int *min, int *max, int *deflt) const override;
    QString getPropertyValueLabel(const PropertyName &,
                                          int value) const override;
    void setProperty(const PropertyName &, int value) override;

    ColourSignificance getLayerColourSignificance() const override {
        return ColourAbsent;
    }

    bool isLayerScrollable(const LayerGeometryProvider *v) const override;

    bool isLayerEditable() const override { return true; }

    int getCompletion(LayerGeometryProvider *) const override { return m_model->getCompletion(); }

    bool getValueExtents(double &min, double &max,
                                 bool &logarithmic, QString &unit) const override;

    void toXml(QTextStream &stream, QString indent = "",
                       QString extraAttributes = "") const override;

    int getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &) const override { return 0; }

    void setLayerDormant(const LayerGeometryProvider *v, bool dormant) override;

    void setProperties(const QXmlAttributes &attributes) override;

    virtual bool addImage(sv_frame_t frame, QString url); // using a command

protected slots:
    void checkAddSources();
    void fileSourceReady();

protected:
    ImageModel::PointList getLocalPoints(LayerGeometryProvider *v, int x, int y) const;

    bool getImageOriginalSize(QString name, QSize &size) const;
    QImage getImage(LayerGeometryProvider *v, QString name, QSize maxSize) const;

    void drawImage(LayerGeometryProvider *v, QPainter &paint, const ImageModel::Point &p,
                   int x, int nx) const;

    //!!! how to reap no-longer-used images?

    typedef std::map<QString, QImage> ImageMap;
    typedef std::map<const LayerGeometryProvider *, ImageMap> ViewImageMap;
    typedef std::map<QString, FileSource *> FileSourceMap;

    static ImageMap m_images;
    static QMutex m_imageMapMutex;
    mutable ViewImageMap m_scaled;
    mutable FileSourceMap m_fileSources;

    QString getLocalFilename(QString img) const;
    void checkAddSource(QString img) const;

    ImageModel *m_model;
    bool m_editing;
    QPoint m_editOrigin;
    ImageModel::Point m_originalPoint;
    ImageModel::Point m_editingPoint;
    ImageModel::EditCommand *m_editingCommand;

    void finish(ImageModel::EditCommand *command) {
        Command *c = command->finish();
        if (c) CommandHistory::getInstance()->addCommand(c, false);
    }
};

#endif

