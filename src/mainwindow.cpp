/*
 * This file is part of PWTUninstall.
 * Copyright (C) 2025 kylon
 *
 * PWTUninstall is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PWTUninstall is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "pwtWin32/win.h"

#include <shellapi.h>
#include <QVBoxLayout>
#include <QLabel>
#include <QDir>
#include <QStandardPaths>
#include <QScroller>
#include <QScrollBar>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "UninstallWorker.h"

MainWindow::MainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    QVBoxLayout *lyt = new QVBoxLayout();
    QHBoxLayout *btnLyt = new QHBoxLayout();
    QLabel *title = new QLabel("PowerTuner Uninstall");
    QFont mainFont = font();
    QFont titleFnt = title->font();

    uninstallBtn = new QPushButton("Uninstall");
    deleteDataChk = new QCheckBox("Delete settings and profiles");
    exitBtn = new QPushButton("Exit");
    logsTx = new QTextEdit();

    mainFont.setPointSize(11);
    setWindowTitle("PowerTuner Uninstall");
    setWindowIcon(QIcon(":/ico/pwt"));
    setMinimumSize(600, 500);
    setMaximumSize(800, 700);
    resize(600, 500);
    setFont(mainFont);

    titleFnt.setBold(true);
    titleFnt.setPointSize(18);
    title->setFont(titleFnt);
    title->setAlignment(Qt::AlignCenter);
    logsTx->setReadOnly(true);
    logsTx->setMinimumHeight(200);
    logsTx->setVisible(false);
    QScroller::grabGesture(logsTx->viewport(), QScroller::LeftMouseButtonGesture);

    btnLyt->addStretch();
    btnLyt->addWidget(uninstallBtn);
    btnLyt->addSpacing(4);
    btnLyt->addWidget(exitBtn);
    lyt->addWidget(title);
    lyt->addSpacing(15);
    lyt->addWidget(deleteDataChk);
    lyt->addWidget(logsTx);
    lyt->addStretch();
    lyt->addLayout(btnLyt);

    ui->centralwidget->setLayout(lyt);

    QObject::connect(uninstallBtn, &QPushButton::clicked, this, &MainWindow::onUninstallBtnClicked);
    QObject::connect(exitBtn, &QPushButton::clicked, this, &MainWindow::onExitBtnClicked);
}

MainWindow::~MainWindow() {
    if (uninstallThd) {
        uninstallThd->quit();
        uninstallThd->wait();
        delete uninstallThd;
    }

    delete ui;
}

void MainWindow::saveLog() const {
    const QString uninstallLogPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QDateTime dtime = QDateTime::currentDateTime();
    QFile logs {QString("%1/uninstall_log.txt").arg(uninstallLogPath)};

    if (!QDir().exists(uninstallLogPath) && !QDir().mkdir(uninstallLogPath))
        return;

    if (!logs.open(QFile::WriteOnly | QFile::Text))
        return;

    logs.write(dtime.toString("ddd MMMM d yyyy - HH:mm").toUtf8());
    logs.write("\n");
    logs.write(logsTx->toPlainText().toUtf8());
    logs.flush();
    logs.close();
}

void MainWindow::onUninstallBtnClicked() {
    PWTU::UninstallWorker *uninstallWorker = new PWTU::UninstallWorker();

    uninstallThd = new QThread();

    uninstallWorker->moveToThread(uninstallThd);
    deleteDataChk->setVisible(false);
    logsTx->setVisible(true);
    uninstallBtn->setVisible(false);
    exitBtn->setEnabled(false);

    QObject::connect(uninstallThd, &QThread::finished, uninstallWorker, &QObject::deleteLater);
    QObject::connect(uninstallThd, &QThread::started, uninstallWorker, &PWTU::UninstallWorker::doUninstall);
    QObject::connect(uninstallWorker, &PWTU::UninstallWorker::logSent, this, &MainWindow::onUninstallLogSent);
    QObject::connect(uninstallWorker, &PWTU::UninstallWorker::resultReady, this, &MainWindow::onUninstallResult);

    uninstallThd->start();
}

void MainWindow::onExitBtnClicked() const {
    QApplication::quit();
}

void MainWindow::onUninstallLogSent(const QString &msg) const {
    logsTx->append(msg);
}

void MainWindow::onUninstallResult(const bool res, const QString &installPath) {
    if (res) {
       const std::wstring args = QString(R"(Start-Sleep -Seconds 2; Remove-Item \"%1\" -recurse)").arg(installPath).toStdWString();
        SHELLEXECUTEINFO execInfo {
            .cbSize = sizeof(SHELLEXECUTEINFO),
            .fMask = SEE_MASK_ASYNCOK,
            .hwnd = nullptr,
            .lpVerb = L"runas",
            .lpFile = L"powershell.exe",
            .lpParameters = args.c_str(),
            .lpDirectory = nullptr,
            .nShow = SW_HIDE,
            .hInstApp = nullptr
        };

        uninstallThd->quit();
        uninstallThd->wait();
        delete uninstallThd;
        uninstallThd = nullptr;

        if (deleteDataChk->isChecked()) {
            const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            QDir clientData {QString("%1/PowerTunerClient").arg(appDataPath)};
            QDir consoleData {QString("%1/PowerTunerConsole").arg(appDataPath)};
            QDir cliData {QString("%1/PowerTunerCLI").arg(appDataPath)};
            QDir daemonData {R"(C:\ProgramData\PowerTunerDaemon)"};

            if (!clientData.removeRecursively())
                logsTx->append("failed to delete desktop client user data");

            if (!consoleData.removeRecursively())
                logsTx->append("failed to delete console client user data");

            if (!cliData.removeRecursively())
                logsTx->append("failed to delete cli client user data");

            if (!daemonData.removeRecursively())
                logsTx->append("failed to delete service user data");
        }

        if (ShellExecuteEx(&execInfo) == FALSE) {
            exitBtn->setEnabled(true);
            logsTx->append(QString("failed to complete uninstall, code %1").arg(GetLastError()));
            logsTx->verticalScrollBar()->setValue(logsTx->verticalScrollBar()->maximum());
            saveLog();

        } else {
            saveLog();
            QThread::msleep(1200);
            QApplication::quit();
        }
    }
}
