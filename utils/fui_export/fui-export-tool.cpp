/*
 * OPL Bank Editor by Wohlstand, a free tool for music bank editing
 * Copyright (c) 2018-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * fui-export tool: exports OPL instruments into individual Furnace Tracker
 * instrument (.fui) files. The input can be:
 *   - a bank file (WOPL, OP2, IBK, ...): every non-blank instrument is exported;
 *   - a single instrument file (SBI, SGI, OPLI, ...): exported as one .fui;
 *   - a directory: every supported instrument file inside it is exported.
 */

#include <FileFormats/ffmt_factory.h>
#include <FileFormats/format_furnace_fui.h>
#include <bank.h>
#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <cstdio>

//! Make a name safe to use as (part of) a file name
static QString sanitize(const QString &raw)
{
    QString s = raw.trimmed();
    QString out;
    for(QChar c : s)
    {
        if(c.isLetterOrNumber() || c == '-' || c == '_')
            out.append(c);
        else if(c.isSpace() || c == '.' || c == '/' || c == '\\')
            out.append('_');
        // drop anything else
    }
    return out;
}

//! Build the set of name filters (e.g. "*.sbi") for every openable/importable
//! instrument format known to the editor.
static QStringList instrumentNameFilters()
{
    QStringList filters;
    QList<const FmBankFormatBase *> fmts = FmBankFormatFactory::allInstrumentFormats();
    for(const FmBankFormatBase *p : fmts)
    {
        // Case variant contains both lower- and upper-case masks
        const QStringList masks = p->formatInstExtensionMaskCase().split(' ', Qt::SkipEmptyParts);
        for(const QString &m : masks)
        {
            if(!filters.contains(m))
                filters.append(m);
        }
    }
    return filters;
}

//! Pick a collision-free output file name (without extension) for an instrument
static QString uniqueBase(QSet<QString> &used, QString base)
{
    if(base.isEmpty())
        base = "instrument";

    QString candidate = base;
    int n = 1;
    while(used.contains(candidate.toLower()))
        candidate = QString("%1_%2").arg(base).arg(++n);

    used.insert(candidate.toLower());
    return candidate;
}

//! Write a single instrument as a .fui file; returns true on success
static bool writeFui(FurnaceFUI &fmt, const QDir &outDir,
                     FmBank::Instrument inst, const QString &base, bool isDrum)
{
    QString path = outDir.filePath(base + ".fui");

    FfmtErrCode err = fmt.saveFileInst(path, inst, isDrum);
    if(err != FfmtErrCode::ERR_OK)
    {
        fprintf(stderr, "  ! failed to write %s (error %d)\n",
                path.toUtf8().constData(), (int)err);
        return false;
    }

    printf("  %s\n", path.toUtf8().constData());
    return true;
}

//! Export every non-blank instrument of a loaded bank
static int exportBank(FurnaceFUI &fmt, const QDir &outDir, FmBank &bank,
                      QSet<QString> &used, int *skipped)
{
    int written = 0;

    for(int i = 0; i < bank.countMelodic(); ++i)
    {
        const FmBank::Instrument &ins = bank.Ins_Melodic_box[i];
        if(ins.is_blank)
        {
            ++*skipped;
            continue;
        }
        QString base = QString("mel_%1").arg(i, 3, 10, QChar('0'));
        QString safe = sanitize(QString::fromUtf8(ins.name));
        if(!safe.isEmpty())
            base += "_" + safe;
        if(writeFui(fmt, outDir, ins, uniqueBase(used, base), ins.rhythm_drum_type != 0))
            ++written;
    }

    for(int i = 0; i < bank.countDrums(); ++i)
    {
        const FmBank::Instrument &ins = bank.Ins_Percussion_box[i];
        if(ins.is_blank)
        {
            ++*skipped;
            continue;
        }
        QString base = QString("perc_%1").arg(i, 3, 10, QChar('0'));
        QString safe = sanitize(QString::fromUtf8(ins.name));
        if(!safe.isEmpty())
            base += "_" + safe;
        if(writeFui(fmt, outDir, ins, uniqueBase(used, base), ins.rhythm_drum_type != 0))
            ++written;
    }

    return written;
}

//! Try to open a single file as an instrument; export it if it works.
//! Returns 1 on success, 0 if the file is not a supported instrument.
static int exportInstrumentFile(FurnaceFUI &fmt, const QDir &outDir,
                                const QFileInfo &inInfo, QSet<QString> &used)
{
    FmBank::Instrument ins = FmBank::emptyInst();
    InstFormats recent = InstFormats::FORMAT_INST_UNKNOWN;
    bool isDrum = false;

    // "import = true" lets us also read import-only instrument formats
    FfmtErrCode err = FmBankFormatFactory::OpenInstrumentFile(
                inInfo.absoluteFilePath(), ins, &recent, &isDrum, true);
    if(err != FfmtErrCode::ERR_OK)
        return 0;

    QString base = sanitize(inInfo.completeBaseName());
    if(base.isEmpty())
        base = sanitize(QString::fromUtf8(ins.name));

    return writeFui(fmt, outDir, ins, uniqueBase(used, base),
                    isDrum || ins.rhythm_drum_type != 0) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if(argc < 2 || argc > 3)
    {
        fprintf(stderr,
                "Export OPL instruments into Furnace .fui files.\n\n"
                "Usage: %s <input> [output-directory]\n\n"
                "<input> may be:\n"
                "  - a bank file (WOPL, OP2, IBK, ...): every instrument is exported;\n"
                "  - an instrument file (SBI, SGI, OPLI, ...): exported as one .fui;\n"
                "  - a directory: every supported instrument file inside it is exported.\n\n"
                "If the output directory is omitted, files are written into a directory\n"
                "named '<input>_fui' next to the input.\n",
                argv[0]);
        return 1;
    }

    QCoreApplication app(argc, argv);
    Q_UNUSED(app);

    FmBankFormatFactory::registerAllFormats();

    QString inputPath = QString::fromLocal8Bit(argv[1]);
    QFileInfo inInfo(inputPath);
    if(!inInfo.exists())
    {
        fprintf(stderr, "Input path does not exist: %s\n", argv[1]);
        return 1;
    }

    QString outPath = (argc == 3)
            ? QString::fromLocal8Bit(argv[2])
            : inInfo.absoluteDir().filePath(inInfo.completeBaseName() + "_fui");

    QDir outDir(outPath);
    if(!outDir.exists() && !outDir.mkpath("."))
    {
        fprintf(stderr, "Could not create output directory: %s\n",
                outPath.toUtf8().constData());
        return 1;
    }

    printf("Exporting to: %s\n", outDir.absolutePath().toUtf8().constData());

    FurnaceFUI fmt;
    QSet<QString> used;
    int written = 0, skipped = 0;

    if(inInfo.isDir())
    {
        QDir inDir(inInfo.absoluteFilePath());
        QStringList filters = instrumentNameFilters();
        QFileInfoList entries = inDir.entryInfoList(filters, QDir::Files, QDir::Name);

        printf("Scanning directory '%s': %d candidate file(s)\n",
               inDir.absolutePath().toUtf8().constData(), (int)entries.size());

        for(const QFileInfo &fi : entries)
        {
            int ok = exportInstrumentFile(fmt, outDir, fi, used);
            if(ok)
            {
                written += ok;
            }
            else
            {
                ++skipped;
                fprintf(stderr, "  - skipped (not a supported instrument): %s\n",
                        fi.fileName().toUtf8().constData());
            }
        }
    }
    else
    {
        // A single file: prefer treating it as a (multi-instrument) bank,
        // fall back to a single-instrument file.
        FmBank bank;
        BankFormats recent = BankFormats::FORMAT_UNKNOWN;
        FfmtErrCode errLoad = FmBankFormatFactory::OpenBankFile(inputPath, bank, &recent);

        if(errLoad == FfmtErrCode::ERR_OK)
        {
            printf("Loaded '%s' (%s): %d melodic, %d percussion instrument(s)\n",
                   inInfo.fileName().toUtf8().constData(),
                   FmBankFormatFactory::formatName(recent).toUtf8().constData(),
                   bank.countMelodic(), bank.countDrums());
            written = exportBank(fmt, outDir, bank, used, &skipped);
        }
        else
        {
            int ok = exportInstrumentFile(fmt, outDir, inInfo, used);
            if(ok)
            {
                written += ok;
            }
            else
            {
                fprintf(stderr, "Could not load '%s' as a bank or an instrument.\n",
                        inInfo.fileName().toUtf8().constData());
                return 1;
            }
        }
    }

    printf("Done: %d instrument(s) written, %d file/slot(s) skipped.\n",
           written, skipped);

    return (written > 0) ? 0 : 2;
}
