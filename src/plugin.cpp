// Copyright (c) 2017-2025 Manuel Schneider

#include "plugin.h"
#include "ui_configwidget.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <QWidget>
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/standarditem.h>
#include <albert/widgetsutil.h>
ALBERT_LOGGING_CATEGORY("ssh")
using namespace Qt::StringLiterals;
using namespace albert::util;
using namespace albert;
using namespace std;

const QStringList Plugin::icon_urls = {u"xdg:ssh"_s, u":ssh"_s};

static QSet<QString> parseConfigFile(const QString &path)
{
    QSet<QString> hosts;

    if (QFile file(path); file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QStringList fields = in.readLine().split(QChar::Space, Qt::SkipEmptyParts);
            if (fields.size() > 1 && fields[0] == u"Host"_s)
            {
                for (int i = 1; i < fields.size(); ++i)
                    if (!(fields[i].contains(u'*') || fields[i].contains(u'?')))
                        hosts << fields[i];
            }
            else if (fields.size() > 1 && fields[0] == u"Include"_s)
            {
                hosts.unite(parseConfigFile((fields[1][0] == u'~') ? QDir::home().filePath(fields[1]) : fields[1]));
            }
        }
        file.close();
    }
    return hosts;
}

Plugin::Plugin()
{
    auto s = settings();
    restore_ssh_cmdln(s);
    restore_ssh_remote_cmdln(s);

    hosts.unite(parseConfigFile(u"/etc/ssh/config"_s));
    hosts.unite(parseConfigFile(QDir::home().filePath(u".ssh/config"_s)));
    INFO << u"Found %1 ssh hosts."_s.arg(hosts.size());
}

QString Plugin::synopsis(const QString &) const
{ return tr("[user@]<host> [script]"); }

bool Plugin::allowTriggerRemap() const { return false; }

vector<RankItem> Plugin::handleGlobalQuery(const Query &query)
{
    vector<RankItem> r;

    static const QRegularExpression regex_synopsis(uR"(^(?:(\w+)@)?\[?([\w\.-]*)\]?(?:\h+(.*))?$)"_s);
    auto match = regex_synopsis.match(query);
    if (!match.hasMatch())
        return r;

    const auto q_user = match.captured(1);
    const auto q_host = match.captured(2);
    const auto q_cmdln = match.captured(3);

    // CRIT << "----";
    // CRIT << "User:" << match.hasCaptured(1)<< q_user;
    // CRIT << "Host:" << match.hasCaptured(2)<< q_host;
    // CRIT << "Params:" << match.hasCaptured(3)<< q_cmdln;

    // Skip if we have a commandline but no host, otherwise spaces in the query clutter results
    if (match.hasCaptured(3) && (!query.isTriggered() || q_host.isEmpty()))
        return r;

    for (const auto &host : as_const(hosts))
        if (host.startsWith(q_host, Qt::CaseInsensitive))
        {
            auto cmdln = ssh_cmdln_.arg(q_user.isEmpty()
                                        ? host
                                        : u"%1@%2"_s.arg(q_user, host),
                                        q_cmdln.isEmpty()
                                            ? ssh_remote_cmdln_.arg("true")  // Fake script doing nothing. Seems more robust and flexible than '$SHELL -i || true'
                                            : ssh_remote_cmdln_.arg(q_cmdln));

            r.emplace_back(
                StandardItem::make(host,
                                   host,
                                   ui_strings.ssh_host,
                                   icon_urls,
                                   {{
                                     u"c"_s,
                                     q_cmdln.isEmpty()
                                        ? ui_strings.connect
                                        : ui_strings.run,
                                     [cmdln, this]{ apps->runTerminal(cmdln); }
                                   }},
                                   u""_s), // Disable completion
                (double)q_host.size() / host.size()
                );
        }

    return r;
}

QWidget *Plugin::buildConfigWidget()
{
    auto *w = new QWidget;
    Ui::ConfigWidget ui;
    ui.setupUi(w);

    ui.formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);


    util::bind(ui.lineEdit_cmdln, this,
               &Plugin::ssh_cmdln,
               &Plugin::set_ssh_cmdln,
               &Plugin::ssh_cmdln_changed);

    connect(ui.lineEdit_cmdln, &QLineEdit::editingFinished,
            this, [this, le=ui.lineEdit_cmdln]
            { if (le->text().isEmpty()) reset_ssh_cmdln(); });

    ui.lineEdit_cmdln->setPlaceholderText(ssh_cmdln_default());


    util::bind(ui.lineEdit_remote_cmdln, this,
               &Plugin::ssh_remote_cmdln,
               &Plugin::set_ssh_remote_cmdln,
               &Plugin::ssh_remote_cmdln_changed);

    connect(ui.lineEdit_remote_cmdln, &QLineEdit::editingFinished,
            this, [this, le=ui.lineEdit_remote_cmdln]
            { if (le->text().isEmpty()) reset_ssh_remote_cmdln(); });

    ui.lineEdit_remote_cmdln->setPlaceholderText(ssh_remote_cmdln_default());


    return w;
}
