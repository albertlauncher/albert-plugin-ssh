// Copyright (c) 2017-2024 Manuel Schneider

#pragma once
#include <QSet>
#include <QString>
#include <albert/extensionplugin.h>
#include <albert/globalqueryhandler.h>
#include <albert/plugin/applications.h>
#include <albert/plugindependency.h>
#include <albert/property.h>

class Plugin : public albert::util::ExtensionPlugin,
               public albert::GlobalQueryHandler
{
    ALBERT_PLUGIN

public:

    Plugin();
    QString synopsis(const QString&) const override;
    bool allowTriggerRemap() const override;
    std::vector<albert::RankItem> handleGlobalQuery(const albert::Query&) override;
    QWidget* buildConfigWidget() override;

private:

    albert::util::StrongDependency<applications::Plugin> apps{QStringLiteral("applications")};
    QSet<QString> hosts;
    static const QStringList icon_urls;

    // Notes:
    // - -t: No TUI without.
    // - || exec $SHELL: Gives the user the chance to read ssh errors.
    ALBERT_PLUGIN_PROPERTY(QString, ssh_cmdln,
                           QStringLiteral(R"(ssh -t %1 %2 || exec $SHELL)"))

    // Notes:
    // - Quotes: I/O errors for zsh, lacking job control for bash otherwise. Anyway $SHELL should
    //   be expanded on the remote host.
    // - $SHELL -i -c …: Running '%1 ; exec $SHELL -i' directly does not run an interactive shell.
    // - exec $SHELL -i: Needed to get an interactive shell after the command has been run.
    // - || true: Avoids returning exit codes to the local shell.
    ALBERT_PLUGIN_PROPERTY(QString, ssh_remote_cmdln,
                           QStringLiteral(R"('$SHELL -i -c "%1 ; exec $SHELL" || true')"))

    struct {
        QString ssh_host = tr("SSH host");
        //: Action
        QString connect = tr("Connect");
        //: Action
        QString run = tr("Run");
    } ui_strings;

};
