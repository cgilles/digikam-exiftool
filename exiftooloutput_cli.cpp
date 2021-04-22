/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2013-11-28
 * Description : a command line tool to test ExifTool output without Exiv2 trnaslation.
 *
 * Copyright (C) 2012-2021 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

// Qt includes

#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>
#include <QVariant>
#include <QObject>

// Local includes

#include "exiftoolparser.h"

using namespace Digikam;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    if (argc != 2)
    {
        qDebug() << "exiftooloutpu_cli - CLI tool to print ExifTool output without Exiv2 translation";
        qDebug() << "Usage: <image>";
        return -1;
    }

    // Create ExifTool parser instance.

    ExifToolParser* const parser = new ExifToolParser();
    parser->setTranslations(false);

    // Read metadata from the file. Start ExifToolParser

    if (!parser->load(QString::fromUtf8(argv[1])))
    {
        return -1;
    }

    QString path                    = parser->currentParsedPath();
    ExifToolParser::TagsMap parsed  = parser->currentParsedTags();

    qDebug().noquote() << "Source File:" << path;

    // Print returned and sorted tags.

    QString     output;
    QTextStream stream(&output);
    QStringList tagsLst;

    const int section1 = -40;   // ExifTool Tag name simplified
    const int section2 = -30;   // Tag value as string.
    QString sep        = QString().fill(QLatin1Char('-'), qAbs(section1 + section2) + 4);

    // Header

    stream << sep
           << endl
           << QString::fromLatin1("%1").arg(QLatin1String("ExifTool::group0.name"), section1) << " | "
           << QString::fromLatin1("%1").arg(QLatin1String("String Value"),          section2)
           << endl
           << sep
           << endl;

    for (ExifToolParser::TagsMap::const_iterator it = parsed.constBegin() ;
         it != parsed.constEnd() ; ++it)
    {
        QString tagNameExifTool = it.key().section(QLatin1Char('.'), 0, 0) +
                                  QLatin1Char('.')                         +
                                  it.key().section(QLatin1Char('.'), -1);
        QString tagType         = it.value()[2].toString();
        QString data            = it.value()[1].toString();

        if (data.size() > -section2)
        {
            data = data.left(-section2 - 3) + QLatin1String("...");
        }

        tagsLst
                << QString::fromLatin1("%1 | %2")
                .arg(tagNameExifTool, section1)
                .arg(data,            section2)
               ;
    }

    tagsLst.sort();

    foreach (const QString& tag, tagsLst)
    {
        stream << tag << endl;
    }

    stream << sep << endl;

    qDebug().noquote() << output;

    return 0;
}
