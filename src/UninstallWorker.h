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

#include <QObject>

namespace PWTU {
    class UninstallWorker final: public QObject {
        Q_OBJECT

    private:
        [[nodiscard]] bool deleteUninstallReg();
        void deleteStartMenuLink();
        void deleteFromPATH();
        void removeDefenderException(const QString &installPath);
        void deleteStartupClient();

    public slots:
        void doUninstall();

    signals:
        void logSent(const QString &msg);
        void resultReady(bool res, const QString &installPath = "");
    };
}
