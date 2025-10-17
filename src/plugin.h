// Copyright (c) 2017-2024 Manuel Schneider

#pragma once
#include <QSet>
#include <QString>
#include <albert/extensionplugin.h>
#include <albert/globalqueryhandler.h>
#include <albert/plugin/applications.h>
#include <albert/plugindependency.h>

class Plugin : public albert::ExtensionPlugin,
               public albert::GlobalQueryHandler
{
    ALBERT_PLUGIN

public:

    Plugin();
    QString synopsis(const QString&) const override;
    bool allowTriggerRemap() const override;
    std::vector<albert::RankItem> rankItems(albert::QueryContext &) override;
    QWidget* buildConfigWidget() override;

    const QString& sshCommandline() const;
    void setSshCommandline(const QString&);

    const QString& sshRemoteCommandline() const;
    void setSshRemoteCommandline(const QString&);

private:

    albert::StrongDependency<applications::Plugin> apps{QStringLiteral("applications")};
    QSet<QString> hosts;

    QString ssh_commandline_;
    QString ssh_remote_commandline_;

    struct {
        QString ssh_host = tr("SSH host");
        //: Action
        QString connect = tr("Connect");
        //: Action
        QString run = tr("Run");
    } ui_strings;

};
