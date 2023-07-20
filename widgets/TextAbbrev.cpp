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

#include "TextAbbrev.h"

#include <QFontMetrics>
#include <QApplication>

#include <iostream>
#include <map>

using std::set;
using std::map;

QString
TextAbbrev::getDefaultEllipsis()
{
    return "...";
}

int
TextAbbrev::getFuzzLength(QString ellipsis)
{
    int len = ellipsis.length();
    if (len < 3) return len + 3;
    else if (len > 5) return len + 5;
    else return len * 2;
}

int
TextAbbrev::getFuzzWidth(const QFontMetrics &metrics, QString ellipsis)
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    int width = metrics.width(ellipsis);
    return width * 2;
}

QString
TextAbbrev::abbreviateTo(QString text, int characters, Policy policy,
                         QString ellipsis)
{
    switch (policy) {

    case ElideEnd:
    case ElideEndAndCommonPrefixes:
        text = text.left(characters) + ellipsis;
        break;
        
    case ElideStart:
        text = ellipsis + text.right(characters);
        break;

    case ElideMiddle:
        if (characters > 2) {
            text = text.left(characters/2 + 1) + ellipsis
                + text.right(characters - (characters/2 + 1));
        } else {
            text = text.left(characters) + ellipsis;
        }
        break;
    }

    return text;
}

QString
TextAbbrev::abbreviate(QString text, int maxLength, Policy policy, bool fuzzy,
                       QString ellipsis)
{
    if (ellipsis == "") ellipsis = getDefaultEllipsis();
    int fl = (fuzzy ? getFuzzLength(ellipsis) : 0);
    if (maxLength <= ellipsis.length()) maxLength = ellipsis.length() + 1;
    if (text.length() <= maxLength + fl) return text;

    int truncated = maxLength - ellipsis.length();
    return abbreviateTo(text, truncated, policy, ellipsis);
}

QString
TextAbbrev::abbreviate(QString text,
                       const QFontMetrics &metrics, int &maxWidth,
                       Policy policy, QString ellipsis)
{
    if (ellipsis == "") ellipsis = getDefaultEllipsis();

    int tw = metrics.width(text);

    if (tw <= maxWidth) {
        maxWidth = tw;
        return text;
    }

    int truncated = text.length();
    QString original = text;

    while (tw > maxWidth && truncated > 1) {

        truncated--;

        if (truncated > ellipsis.length()) {
            text = abbreviateTo(original, truncated, policy, ellipsis);
        } else {
            break;
        }

        tw = metrics.width(text);
    }

    maxWidth = tw;
    return text;
}

QStringList
TextAbbrev::abbreviate(const QStringList &texts, int maxLength,
                       Policy policy, bool fuzzy, QString ellipsis)
{
    if (policy == ElideEndAndCommonPrefixes &&
        texts.size() > 1) {

        if (ellipsis == "") ellipsis = getDefaultEllipsis();
        int fl = (fuzzy ? getFuzzLength(ellipsis) : 0);
        if (maxLength <= ellipsis.length()) maxLength = ellipsis.length() + 1;

        int maxOrigLength = 0;
        for (int i = 0; i < texts.size(); ++i) {
            int len = texts[i].length();
            if (len > maxOrigLength) maxOrigLength = len;
        }
        if (maxOrigLength <= maxLength + fl) return texts;

        return abbreviate(elidePrefixes
                          (texts, maxOrigLength - maxLength, ellipsis),
                          maxLength, ElideEnd, fuzzy, ellipsis);
    }

    QStringList results;
    for (int i = 0; i < texts.size(); ++i) {
        results.push_back
            (abbreviate(texts[i], maxLength, policy, fuzzy, ellipsis));
    }
    return results;
}

QStringList
TextAbbrev::abbreviate(const QStringList &texts, const QFontMetrics &metrics,
                       int &maxWidth, Policy policy, QString ellipsis)
{
    if (policy == ElideEndAndCommonPrefixes &&
        texts.size() > 1) {

        if (ellipsis == "") ellipsis = getDefaultEllipsis();

        int maxOrigWidth = 0;
        for (int i = 0; i < texts.size(); ++i) {
            int w = metrics.width(texts[i]);
            if (w > maxOrigWidth) maxOrigWidth = w;
        }

        return abbreviate(elidePrefixes(texts, metrics,
                                        maxOrigWidth - maxWidth, ellipsis),
                          metrics, maxWidth, ElideEnd, ellipsis);
    }

    QStringList results;
    int maxAbbrWidth = 0;
    for (int i = 0; i < texts.size(); ++i) {
        int width = maxWidth;
        QString abbr = abbreviate(texts[i], metrics, width, policy, ellipsis);
        if (width > maxAbbrWidth) maxAbbrWidth = width;
        results.push_back(abbr);
    }
    maxWidth = maxAbbrWidth;
    return results;
}

static QStringList
replacePrefixes(const QStringList &texts,
                const map<QString, QString> &replacements)
{
    QStringList results;
    for (auto text : texts) {
        bool found = false;
        for (auto p : replacements) {
            if (text.startsWith(p.first)) {
                results.push_back
                    (p.second +
                     text.right(text.length() - p.first.length()));
                found = true;
                break;
            }
        }
        if (!found) {
            results.push_back(text);
        }
    }

    return results;
}

QStringList
TextAbbrev::elidePrefixes(const QStringList &texts,
                          int targetReduction,
                          QString ellipsis)
{
    if (texts.empty()) return texts;

    auto prefixes = getCommonPrefixes(texts);
    int fl = getFuzzLength(ellipsis);

    map<QString, QString> reduced;
    
    for (auto pfx : prefixes) {

        int plen = pfx.length();
        if (plen < fl) continue;

        int truncated = plen;
        if (plen >= targetReduction + fl) {
            truncated = plen - targetReduction;
        } else {
            truncated = fl;
        }

        reduced[pfx] = abbreviate(pfx, truncated, ElideEnd, false, ellipsis);
    }

    return replacePrefixes(texts, reduced);
}    

QStringList
TextAbbrev::elidePrefixes(const QStringList &texts,
                          const QFontMetrics &metrics,
                          int targetWidthReduction,
                          QString ellipsis)
{
    if (texts.empty()) return texts;

    auto prefixes = getCommonPrefixes(texts);
    int fl = getFuzzLength(ellipsis);

    map<QString, QString> reduced;
    
    for (auto pfx : prefixes) {

        int plen = pfx.length();
        if (plen < fl) continue;

        int pwid = metrics.width(pfx);
        int twid = pwid - targetWidthReduction;
        if (twid < metrics.width(ellipsis) * 2) {
            twid = metrics.width(ellipsis) * 2;
        }
        reduced[pfx] = abbreviate(pfx, metrics, twid, ElideEnd, ellipsis);
    }

    return replacePrefixes(texts, reduced);
}

static bool
isCommonPrefix(QString prefix, const QStringList &texts)
{
    int n = 0;
    for (auto text : texts) {
        if (text.startsWith(prefix)) {
            if (++n > 1) {
                return true;
            }
        }
    }
    return false;
}

set<QString>
TextAbbrev::getCommonPrefixes(const QStringList &texts)
{
    set<QString> prefixes;
    QString splitChars(";:,./#-!()$_+=[]{}\\");
    
    for (auto text : texts) {

        if (isCommonPrefix(text, texts)) {
            prefixes.insert(text);
            continue;
        }

        int candidate = text.length();

        while (--candidate > 1) {
            if (splitChars.contains(text[candidate])) {
                QString prefix = text.left(candidate);
                if (isCommonPrefix(prefix, texts)) {
                    prefixes.insert(prefix);
                    break;
                }
            }
        }
    }

    return prefixes;
}

