/*
    SPDX-FileCopyrightText: 2016 David Edmundson <davidedmundson@kde.org>
    SPDX-FileCopyrightText: 2020 Piotr Henryk Dabrowski <phd@phd.re>
    SPDX-FileCopyrightText: 2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "qtquicksettings.h"
#include "renderersettings.h"

#include <QGuiApplication>
#include <QLibraryInfo>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QVersionNumber>

/**
 * If QtQuick is configured (QQuickWindow::sceneGraphBackend()) to use the OpenGL backend,
 * check if it is supported or otherwise reconfigure QtQuick to fallback to software mode.
 * This function is called by init().
 *
 * @returns true if the selected backend is supported, false on fallback to software mode.
 */
static bool checkBackend(QOpenGLContext &checkContext)
{
    if (!QQuickWindow::sceneGraphBackend().isEmpty()) {
        return true; // QtQuick is not configured to use the OpenGL backend
    }

    // kwin wayland has it's own QPA, it is unable to create a GL context at this point.
    // KF6 TODO, drop this . The issue will be resolved in future kwin releases.
    QString platformName = qApp->platformName();
    if (platformName == QLatin1String("wayland-org.kde.kwin.qpa")) {
        return true;
    }

#ifdef QT_NO_OPENGL
    bool ok = false;
#else
    bool ok = checkContext.create();
#endif
    if (!ok) {
        qWarning("Warning: fallback to QtQuick software backend.");
        QQuickWindow::setSceneGraphBackend(QStringLiteral("software"));
    }
    return ok;
}

void KQuickAddons::QtQuickSettings::init()
{
    if (!(qobject_cast<QGuiApplication *> qApp)) {
        qWarning("Error: cannot call KQuickAddons::QtQuickSettings::init() before QGuiApplication is created.");
        return;
    }

    PlasmaQtQuickSettings::RendererSettings s;
    QOpenGLContext checkContext;
    if (!s.sceneGraphBackend().isEmpty()) {
        QQuickWindow::setSceneGraphBackend(s.sceneGraphBackend());
    } else {
        QQuickWindow::setSceneGraphBackend(QStringLiteral(""));
        checkBackend(checkContext);
    }

    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP")) {
        if (!s.renderLoop().isEmpty()) {
            qputenv("QSG_RENDER_LOOP", s.renderLoop().toLatin1());
        } else if (QGuiApplication::platformName() == QLatin1String("wayland")) {
            // Workaround for Bug 432062 / QTBUG-95817
            QOffscreenSurface surface;
            surface.create();
            if (checkContext.makeCurrent(&surface)) {
                const char *vendor = reinterpret_cast<const char *>(checkContext.functions()->glGetString(GL_VENDOR));
                if (qstrcmp(vendor, "NVIDIA Corporation") == 0) {
                    // Otherwise Qt Quick Windows break when resized
                    qputenv("QSG_RENDER_LOOP", "basic");
                }
            }
        }
    }

    auto format = QSurfaceFormat::defaultFormat();
    if (s.forceGlCoreProfile()) {
        format.setVersion(3, 2);
        format.setProfile(QSurfaceFormat::CoreProfile);
    }
    // Before Qt 5.12.2 this setting was somewhat unstable
    // it was opt-in to find bugs both in KDE and Qt
    // For 5.13 with modern plasma it should be fine
    if (s.graphicsResetNotifications() || QLibraryInfo::version() >= QVersionNumber(5, 13, 0)) {
        format.setOption(QSurfaceFormat::ResetNotification);
    }
    QSurfaceFormat::setDefaultFormat(format);
}
