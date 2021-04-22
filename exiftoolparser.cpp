/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2013-11-28
 * Description : ExifTool JSON parser
 *
 * Copyright (C) 2013-2021 by Gilles Caulier <caulier dot gilles at gmail dot com>
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

#include "exiftoolparser.h"

// Qt includes

#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEventLoop>
#include <QDebug>

// Local includes

#include "exiftoolprocess.h"

namespace Digikam
{

class Q_DECL_HIDDEN ExifToolParser::Private
{
public:

    explicit Private()
      : translate(true),
        proc     (nullptr),
        loop     (nullptr)
    {
    }

    bool             translate;
    ExifToolProcess* proc;
    QEventLoop*      loop;
    QString          parsedPath;
    TagsMap          parsedMap;
    TagsMap          ignoredMap;
};

ExifToolParser::ExifToolParser(QObject* const parent)
    : QObject(parent),
      d      (new Private)
{
/*
    // Post creation of hash tables for tag translations

    ExifToolTranslator::instance();
*/
    // Create ExifTool parser instance.

    d->proc = new ExifToolProcess(parent);
/*
    connect(MetaEngineSettings::instance(), SIGNAL(signalSettingsChanged()),
            this, SLOT(slotMetaEngineSettingsChanged()));
*/
    slotMetaEngineSettingsChanged();
}

ExifToolParser::~ExifToolParser()
{
    if (d->loop)
    {
        d->loop->quit();
        delete d->loop;
    }

    d->proc->terminate();

    delete d->proc;
    delete d;
}

void ExifToolParser::setTranslations(bool b)
{
    d->translate = b;
}

QString ExifToolParser::currentParsedPath() const
{
    return d->parsedPath;
}

ExifToolParser::TagsMap ExifToolParser::currentParsedTags() const
{
    return d->parsedMap;
}

ExifToolParser::TagsMap ExifToolParser::currentIgnoredTags() const
{
    return d->ignoredMap;
}

QString ExifToolParser::currentErrorString() const
{
    return d->proc->errorString();
}

bool ExifToolParser::load(const QString& path)
{
    d->parsedPath.clear();
    d->parsedMap.clear();
    d->ignoredMap.clear();

    QFileInfo fileInfo(path);

    if (!fileInfo.exists())
    {
        return false;
    }

    // Read metadata from the file. Start ExifToolProcess

    d->proc->start();

    if (!d->proc->waitForStarted(500))
    {
        d->proc->kill();
        qWarning() << "ExifTool process cannot be started (" << d->proc->program() << ")";

        return false;
    }

    // Build command (get metadata as JSON array)

    QByteArrayList cmdArgs;
    cmdArgs << QByteArray("-json");
    cmdArgs << QByteArray("-binary");
    cmdArgs << QByteArray("-G:0:1:2:4:6");
    cmdArgs << QByteArray("-n");
    cmdArgs << QByteArray("-l");
    cmdArgs << QDir::toNativeSeparators(fileInfo.filePath()).toUtf8();

    // Send command to ExifToolProcess

    int ret = d->proc->command(cmdArgs); // See additional notes

    if (ret == 0)
    {
        qWarning() << "ExifTool parsing command cannot be sent";

        return false;
    }

    d->loop = new QEventLoop(this);

    // Connect at cmdCompleted signal to slot

    QList<QMetaObject::Connection> hdls;

    hdls << connect(d->proc, &ExifToolProcess::signalCmdCompleted,
                    this, &ExifToolParser::slotCmdCompleted);

    hdls << connect(d->proc, &ExifToolProcess::signalErrorOccurred,
                    this, &ExifToolParser::slotErrorOccurred);

    hdls << connect(d->proc, &ExifToolProcess::signalFinished,
                    this, &ExifToolParser::slotFinished);

    d->loop->exec();

    foreach (QMetaObject::Connection hdl, hdls)
    {
        disconnect(hdl);
    }

    return true;
}

void ExifToolParser::slotCmdCompleted(int /*cmdId*/,
                                      int /*execTime*/,
                                      const QByteArray& stdOut,
                                      const QByteArray& /*stdErr*/)
{
    // Convert JSON array as QVariantMap

    QJsonDocument jsonDoc     = QJsonDocument::fromJson(stdOut);
    QJsonArray    jsonArray   = jsonDoc.array();
    QJsonObject   jsonObject  = jsonArray.at(0).toObject();
    QVariantMap   metadataMap = jsonObject.toVariantMap();

    for (QVariantMap::const_iterator it = metadataMap.constBegin() ;
        it != metadataMap.constEnd() ; ++it)
    {
        QString     tagNameExifTool;
        QString     tagType;
        QStringList sections  = it.key().split(QLatin1Char(':'));

        if      (sections.size() == 5)
        {
            tagNameExifTool = QString::fromLatin1("%1.%2.%3.%4")
                                  .arg(sections[0])
                                  .arg(sections[1])
                                  .arg(sections[2])
                                  .arg(sections[4]);
            tagType         = sections[3];
        }
        else if (sections.size() == 4)
        {
            tagNameExifTool = QString::fromLatin1("%1.%2.%3.%4")
                                  .arg(sections[0])
                                  .arg(sections[1])
                                  .arg(sections[2])
                                  .arg(sections[3]);
        }
        else if (sections[0] == QLatin1String("SourceFile"))
        {
            d->parsedPath = it.value().toString();
            continue;
        }
        else
        {
            continue;
        }

        QVariantMap propsMap = it.value().toMap();
        QString data         = propsMap.find(QLatin1String("val")).value().toString();
        QString desc         = propsMap.find(QLatin1String("desc")).value().toString();

        if (d->translate)
        {
/*
            // Translate ExifTool tag names to Exiv2 scheme

            if (ExifToolTranslator::instance()->isIgnoredGroup(tagNameExifTool))
            {
                if (!tagNameExifTool.startsWith(QLatin1String("...")))
                {
                    d->ignoredMap.insert(tagNameExifTool, QVariantList() << QString() << data << tagType);
                }

                continue;
            }

            // Tags to translate To Exiv2 naming scheme.

            QString tagNameExiv2 = ExifToolTranslator::instance()->translateToExiv2(tagNameExifTool);
            QVariant var;

            if      (tagNameExiv2.startsWith(QLatin1String("Exif.")))
            {
                if      (tagType == QLatin1String("string"))
                {
                    var = data;
                }
                else if (
                         (tagType == QLatin1String("int8u"))  ||
                         (tagType == QLatin1String("int16u")) ||
                         (tagType == QLatin1String("int32u")) ||
                         (tagType == QLatin1String("int8s"))  ||
                         (tagType == QLatin1String("int16s")) ||
                         (tagType == QLatin1String("int32s"))
                        )
                {
                    var = data.toLongLong();
                }
                else if (tagType == QLatin1String("undef"))
                {
                    if (
                        (tagNameExiv2 == QLatin1String("Exif.Photo.ComponentsConfiguration")) ||
                        (tagNameExiv2 == QLatin1String("Exif.Photo.SceneType"))               ||
                        (tagNameExiv2 == QLatin1String("Exif.Photo.FileSource"))
                       )
                    {
                        QByteArray conv;
                        QStringList vals = data.split(QLatin1Char(' '));

                        foreach (const QString& v, vals)
                        {
                            conv.append(QString::fromLatin1("0x%1").arg(v.toInt(), 2, 16).toLatin1());
                        }

                        var = QByteArray::fromHex(conv);
                    }
                    else
                    {
                        var = data.toLatin1();
                    }
                }
                else if (
                         (tagType == QLatin1String("double"))      ||
                         (tagType == QLatin1String("float"))       ||
                         (tagType == QLatin1String("rational64s")) ||
                         (tagType == QLatin1String("rational64u"))
                        )
                {
                    var = data.toDouble();
                }
                else
                {
                    d->ignoredMap.insert(tagNameExiv2, QVariantList() << tagNameExifTool << data << tagType);
                }
            }
            else if (tagNameExiv2.startsWith(QLatin1String("Iptc.")))
            {
                var = data;
            }
            else if (tagNameExiv2.startsWith(QLatin1String("Xmp.")))
            {
                var = data;
            }

            d->parsedMap.insert(tagNameExiv2, QVariantList()
                                                 << tagNameExifTool // ExifTool tag name.
                                                 << var             // ExifTool data as variant.
                                                 << tagType         // ExifTool data type.
                                                 << desc);          // ExifTool tag description.
*/
        }
        else
        {
            // Do not translate ExifTool tag names to Exiv2 scheme.

            if (data.startsWith(QLatin1String("base64:")))
            {
                data = QLatin1String("binary data...");
            }

            d->parsedMap.insert(tagNameExifTool, QVariantList()
                                                     << QString()   // Empty Exiv2 tag name.
                                                     << data        // ExifTool Raw data as string.
                                                     << tagType     // ExifTool data type.
                                                     << desc);      // ExifTool tag description.
        }
    }

    if (d->loop)
    {
        d->loop->quit();
    }
}

void ExifToolParser::slotErrorOccurred(QProcess::ProcessError error)
{
    qWarning() << "ExifTool process exited with error:" << error;

    if (d->loop)
    {
        d->loop->quit();
    }
}

void ExifToolParser::slotFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "ExifTool process finished with code:" << exitCode
                                    << "and status" << exitStatus;

    if (d->loop)
    {
        d->loop->quit();
    }
}

void ExifToolParser::slotMetaEngineSettingsChanged()
{
    d->proc->setProgram(
        defaultExifToolSearchPaths().first() +
        QLatin1Char('/') +

#ifdef Q_OS_WIN

        QLatin1String("exiftool.exe")

#else

        QLatin1String("exiftool")

#endif

    );
}

QStringList ExifToolParser::defaultExifToolSearchPaths() const
{
    QStringList defPaths;

#ifdef Q_OS_MACOS

    // Install path for the official ExifTool DMG package
    defPaths << QLatin1String("/usr/local/bin");

    // digiKam Bundle PKG install path
    defPaths << macOSBundlePrefix() + QLatin1String("bin");

    // Std Macports install path
    defPaths << QLatin1String("/opt/local/bin");

#endif

#ifdef Q_OS_WIN

    defPaths << QLatin1String("C:/Program Files/digiKam");

#endif

#ifdef Q_OS_UNIX

    defPaths << QLatin1String("/usr/bin");

#endif

    return defPaths;
}

} // namespace Digikam
