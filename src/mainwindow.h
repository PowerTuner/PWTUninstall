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
#pragma once

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QThread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow final: public QMainWindow {
    Q_OBJECT

private:
    Ui::MainWindow *ui;
    QPushButton *uninstallBtn = nullptr;
    QPushButton *exitBtn = nullptr;
    QTextEdit *logsTx = nullptr;
    QCheckBox *deleteDataChk = nullptr;
    QThread *uninstallThd = nullptr;

    void saveLog() const;

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onUninstallBtnClicked();
    void onExitBtnClicked() const;
    void onUninstallLogSent(const QString &msg) const;
    void onUninstallResult(bool res, const QString &installPath);
};
