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
#include "pwtWin32/win32Reg.h"
#include "pwtWin32/win32Svc.h"

#include <tlhelp32.h>
#include <ktmw32.h>
#include <shellapi.h>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>

#include "UninstallWorker.h"

namespace PWTU {
    bool UninstallWorker::deleteUninstallReg() {
        TCHAR transactionDesc[] = L"PowerTuner uninstall transaction";
        HANDLE transaction;
        LSTATUS ret;

        emit logSent("deleting registry entries..");

        transaction = CreateTransaction(nullptr, nullptr, 0, 0, 0, 0, transactionDesc);
        if (transaction == INVALID_HANDLE_VALUE) {
            emit logSent(QString("failed to create transaction, code %1").arg(GetLastError()));
            return false;
        }

        ret = RegDeleteKeyTransacted(HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PowerTuner)", KEY_WOW64_64KEY, 0, transaction, nullptr);
        if (ret == ERROR_FILE_NOT_FOUND) {
            CloseHandle(transaction);
            return true;

        } else if (ret != ERROR_SUCCESS) {
            emit logSent(QString("failed to open uninstall registry, code %1").arg(ret));
            CloseHandle(transaction);
            return false;
        }

        if (CommitTransaction(transaction) == FALSE) {
            emit logSent(QString("failed to commit uninstall transaction, code %1").arg(GetLastError()));
            goto err_abort;
        }

        CloseHandle(transaction);
        return true;

    err_abort:
        RollbackTransaction(transaction);
        CloseHandle(transaction);
        return false;
    }

    void UninstallWorker::deleteStartMenuLink() {
        const QString startMenuPath = R"(C:\ProgramData\Microsoft\Windows\Start Menu\Programs\PowerTuner)";
        QDir qdir {startMenuPath};

        emit logSent("deleting start menu folder..");

        if (!qdir.exists(startMenuPath))
            return;

        if (!qdir.removeRecursively())
            emit logSent(QString("failed to delete start menu folder: %1").arg(startMenuPath));
    }

    void UninstallWorker::deleteFromPATH() {
        constexpr TCHAR systemEnv[] = LR"(System\CurrentControlSet\Control\Session Manager\Environment)";
        QString sysPath;
        QString usrPath;
        QString errStr;

        if (PWTW32::regReadSZ(HKEY_LOCAL_MACHINE, systemEnv, L"Path", sysPath, errStr)) {
            QList<QString> paths = sysPath.split(';', Qt::SkipEmptyParts);

            for (int i=0,l=paths.size(); i<l; ++i) {
                if (!paths[i].contains("PowerTuner"))
                    continue;

                emit logSent("deleting from system PATH..");
                paths.removeAt(i);

                if (!PWTW32::regWriteSZ(HKEY_LOCAL_MACHINE, systemEnv, L"Path", REG_EXPAND_SZ, paths.join(';'), errStr)) {
                    emit logSent(errStr);
                    return;
                }

                break;
            }
        } else {
            emit logSent(errStr);
            return;
        }

        if (PWTW32::regReadSZ(HKEY_CURRENT_USER, L"Environment", L"Path", usrPath, errStr)) {
            QList<QString> paths = usrPath.split(';', Qt::SkipEmptyParts);

            for (int i=0,l=paths.size(); i<l; ++i) {
                if (!paths[i].contains("PowerTuner"))
                    continue;

                emit logSent("deleting from user PATH..");
                paths.removeAt(i);

                if (!PWTW32::regWriteSZ(HKEY_CURRENT_USER, L"Environment", L"Path", REG_EXPAND_SZ, paths.join(';'), errStr)) {
                    emit logSent(errStr);
                    return;
                }

                break;
            }
        } else {
            emit logSent(errStr);
        }
    }

    void UninstallWorker::removeDefenderException(const QString &installPath) {
        const std::wstring args = QString(R"(Remove-MpPreference -ExclusionPath \"%1\")").arg(installPath).toStdWString();
        SHELLEXECUTEINFO execInfo {
            .cbSize = sizeof(SHELLEXECUTEINFO),
            .fMask = SEE_MASK_NOCLOSEPROCESS,
            .hwnd = nullptr,
            .lpVerb = L"runas",
            .lpFile = L"powershell.exe",
            .lpParameters = args.c_str(),
            .lpDirectory = nullptr,
            .nShow = SW_HIDE,
            .hInstApp = nullptr
        };

        if (ShellExecuteEx(&execInfo) == TRUE && execInfo.hProcess != nullptr) {
            WaitForSingleObject(execInfo.hProcess, 10 * 1000);
            CloseHandle(execInfo.hProcess);
            emit logSent("removing winrin0 exception..");

        } else {
            emit logSent(QString("failed to remove winring0 exception, code %1").arg(GetLastError()));
        }
    }

    void UninstallWorker::deleteStartupClient() {
        constexpr wchar_t runPath[] = LR"(Software\Microsoft\Windows\CurrentVersion\Run)";
        QString errStr;

        emit logSent("deleting from startup..");

        if (!PWTW32::regDeleteVal(HKEY_CURRENT_USER, runPath, L"powertuner", errStr))
            emit logSent(QString("failed to delete startup key for desktop client\n").arg(errStr));

        if (!PWTW32::regDeleteVal(HKEY_CURRENT_USER, runPath, L"powertunerconsole", errStr))
            emit logSent(QString("failed to delete startup key for console client\n").arg(errStr));
    }

    void UninstallWorker::doUninstall() {
        QString installPath;
        QString errStr;

        emit logSent("starting uninstall..");

        if (!PWTW32::regReadSZ(HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PowerTuner)", L"InstallLocation", installPath, errStr))
            goto err_abort;

        if (!PWTW32::stopService(errStr))
            goto err_abort;

        if (!PWTW32::deleteService(errStr))
            goto err_abort;

        if (!deleteUninstallReg()) {
            emit resultReady(false);
            return;
        }

        deleteStartMenuLink();
        deleteFromPATH();
        removeDefenderException(installPath);
        deleteStartupClient();
        QDir(installPath).removeRecursively();

        emit resultReady(true, installPath);
        return;

    err_abort:
        emit logSent(errStr);
        emit resultReady(false);
    }
}
