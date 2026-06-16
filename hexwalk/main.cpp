/*
 * Copyright (C) 2025 Carmix <carmixdev@gmail.com>
 *
 * This file is part of HexWalk.
 *
 * HexWalk is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HexWalk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include <QIcon>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>

#include "hexwalkmain.h"
int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(hexwalk);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("HexWalk");
    app.setOrganizationName("HexWalk");
#ifdef Q_OS_WIN32
    app.setWindowIcon(QIcon(":images/hexwalk.ico"));
#else
    app.setWindowIcon(QIcon(":images/hexwalk64.png"));
#endif
    // set style
    app.setStyle(QStyleFactory::create("Fusion"));
    // increase font size for better reading
    QString fontPath1=":/fonts/Roboto.ttf";
    QString fontPath2=":/fonts/Courier.ttf";
    QFontDatabase::addApplicationFont(fontPath1);
    QFontDatabase::addApplicationFont(fontPath2);
    //if (fontId != -1)
    //{
        QFont font("Roboto");
        font.setPointSize(font.pointSize()-2);
        app.setFont(font);
    //}
    /*QFont defaultFont = QApplication::font();
    defaultFont.setPointSize(defaultFont.pointSize()+2);
    app.setFont(defaultFont);*/
    QPalette workbenchPalette;
    workbenchPalette.setColor(QPalette::Window, QColor("#f4f6f8"));
    workbenchPalette.setColor(QPalette::WindowText, QColor("#18202a"));
    workbenchPalette.setColor(QPalette::Base, QColor("#ffffff"));
    workbenchPalette.setColor(QPalette::AlternateBase, QColor("#eef2f6"));
    workbenchPalette.setColor(QPalette::Text, QColor("#18202a"));
    workbenchPalette.setColor(QPalette::Button, QColor("#f8fafc"));
    workbenchPalette.setColor(QPalette::ButtonText, QColor("#18202a"));
    workbenchPalette.setColor(QPalette::ToolTipBase, QColor("#18202a"));
    workbenchPalette.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    workbenchPalette.setColor(QPalette::Highlight, QColor("#2f6f9f"));
    workbenchPalette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    workbenchPalette.setColor(QPalette::Link, QColor("#2563eb"));
    workbenchPalette.setColor(QPalette::Dark, QColor("#c7d0dc"));
    workbenchPalette.setColor(QPalette::Mid, QColor("#d8dee8"));
    workbenchPalette.setColor(QPalette::Shadow, QColor("#aab4c2"));
    app.setPalette(workbenchPalette);

    app.setStyleSheet(
        "QMainWindow, QDialog { background: #f4f6f8; color: #18202a; }"
        "QMenuBar { background: #f8fafc; border-bottom: 1px solid #d8dee8; padding: 2px; }"
        "QMenuBar::item { padding: 5px 10px; border-radius: 4px; }"
        "QMenuBar::item:selected { background: #e6edf5; }"
        "QMenu { background: #ffffff; border: 1px solid #cfd8e3; padding: 4px; }"
        "QMenu::item { padding: 5px 24px 5px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background: #e8f1fa; color: #12324a; }"
        "QToolBar { background: #f8fafc; border: 0; border-bottom: 1px solid #d8dee8; spacing: 6px; padding: 5px; }"
        "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 4px; padding: 5px; }"
        "QToolButton:hover { background: #e8f1fa; border-color: #c7d8e8; }"
        "QToolButton:pressed { background: #dceaf7; }"
        "QLineEdit { background: #ffffff; border: 1px solid #b9c6d3; border-radius: 4px; padding: 3px 6px; selection-background-color: #2f6f9f; }"
        "QLineEdit:focus { border-color: #2f6f9f; }"
        "QGroupBox { background: #ffffff; border: 1px solid #d4dce6; border-radius: 6px; margin-top: 14px; padding: 10px; font-weight: 600; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #364152; }"
        "QLabel[valueLabel=\"true\"], QLabel[statusValue=\"true\"] { background: #f8fafc; border: 1px solid #d4dce6; border-radius: 4px; padding: 4px 7px; color: #17202a; }"
        "QLabel[fieldLabel=\"true\"] { color: #536172; font-weight: 600; }"
        "QStatusBar { background: #f8fafc; border-top: 1px solid #d8dee8; color: #536172; }"
        "QScrollBar:vertical { background: #edf2f7; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #b7c3d0; min-height: 24px; border-radius: 5px; }"
        "QScrollBar:horizontal { background: #edf2f7; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #b7c3d0; min-width: 24px; border-radius: 5px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
    );


    // Identify locale and load translation if available
    QString locale = QLocale::system().name();
    QTranslator translator;
    translator.load(QString("hexwalk_") + locale);
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.addPositionalArgument("file", "File to open");
    parser.addHelpOption();
    parser.process(app);

    /*MainWindow *mainWin = new MainWindow;
    if(!parser.positionalArguments().isEmpty())
        mainWin->loadFile(parser.positionalArguments().at(0));
    mainWin->show();*/
    HexWalkMain *hexwalkWin = new HexWalkMain;
    if(!parser.positionalArguments().isEmpty())
        hexwalkWin->loadFile(parser.positionalArguments().at(0));
    hexwalkWin->show();

    return app.exec();
}
