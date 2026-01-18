// Copyright (c) 2017-2025 Manuel Schneider

#include "plugin.h"
#include "ui_configwidget.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QRegularExpression>
#include <QSettings>
#include <QString>
#include <QTextStream>
#include <QWidget>
#include <albert/icon.h>
#include <albert/logging.h>
#include <albert/standarditem.h>
#include <albert/widgetsutil.h>
ALBERT_LOGGING_CATEGORY("ssh")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

static const auto ck_ssh_commandline = "ssh_commandline";
static const auto ck_ssh_remote_commandline = "ssh_remote_commandline";
// Notes:
// - -t: No TUI without.
// - || exec $SHELL: Gives the user the chance to read ssh errors.
static const auto default_ssh_commandline = uR"(ssh -t %1 %2 || exec $SHELL)"_s;
// Notes:
// - Quotes: I/O errors for zsh, lacking job control for bash otherwise. Anyway $SHELL should
//   be expanded on the remote host.
// - $SHELL -i -c …: Running '%1 ; exec $SHELL -i' directly does not run an interactive shell.
// - exec $SHELL -i: Needed to get an interactive shell after the command has been run.
// - || true: Avoids returning exit codes to the local shell.
static const auto default_ssh_remote_commandline = uR"('$SHELL -i -c "%1 ; exec $SHELL" || true')"_s;

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
    ssh_commandline_ = s->value(ck_ssh_commandline, default_ssh_commandline).value<QString>();
    ssh_remote_commandline_ = s->value(ck_ssh_remote_commandline, default_ssh_remote_commandline).value<QString>();

    hosts.unite(parseConfigFile(u"/etc/ssh/config"_s));
    hosts.unite(parseConfigFile(QDir::home().filePath(u".ssh/config"_s)));
    INFO << u"Found %1 ssh hosts."_s.arg(hosts.size());
}

QString Plugin::synopsis(const QString &) const
{ return tr("[user@]<host> [script]"); }

bool Plugin::allowTriggerRemap() const { return false; }

vector<RankItem> Plugin::rankItems(QueryContext &ctx)
{
    vector<RankItem> r;

    static const QRegularExpression regex_synopsis(uR"(^(?:(\w+)@)?\[?([\w\.-]*)\]?(?:\h+(.*))?$)"_s);
    auto match = regex_synopsis.match(ctx);
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
    if (match.hasCaptured(3) && (ctx.trigger().isEmpty() || q_host.isEmpty()))
        return r;

    for (const auto &host : as_const(hosts))
        if (host.startsWith(q_host, Qt::CaseInsensitive))
        {
            auto cmdln = ssh_commandline_.arg(q_user.isEmpty() ? host : u"%1@%2"_s.arg(q_user, host),
                                              q_cmdln.isEmpty()
                                                  ? ssh_remote_commandline_.arg(u"true"_s)  // Fake script doing nothing. Seems more robust and flexible than '$SHELL -i || true'
                                                  : ssh_remote_commandline_.arg(q_cmdln));

            r.emplace_back(
                StandardItem::make(host,
                                   host,
                                   ui_strings.ssh_host,
                                   []{ return Icon::image(u":ssh"_s); },
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


    bindWidget(ui.lineEdit_cmdln, this,
               &Plugin::sshCommandline,
               &Plugin::setSshCommandline);

    ui.lineEdit_cmdln->setPlaceholderText(default_ssh_commandline);

    bindWidget(ui.lineEdit_remote_cmdln, this,
               &Plugin::sshCommandline,
               &Plugin::setSshCommandline);

    ui.lineEdit_remote_cmdln->setPlaceholderText(default_ssh_remote_commandline);

    return w;
}

const QString &Plugin::sshCommandline() const { return ssh_commandline_; }

void Plugin::setSshCommandline(const QString &v)
{
    if (v.isEmpty())
    {
        settings()->remove(ck_ssh_commandline);
        ssh_commandline_ = default_ssh_commandline;
    }
    else if (ssh_commandline_ != v)
    {
        settings()->setValue(ck_ssh_commandline, ssh_commandline_);
        ssh_commandline_ = v;
    }
}

const QString &Plugin::sshRemoteCommandline() const { return ssh_remote_commandline_; }

void Plugin::setSshRemoteCommandline(const QString &v)
{
    if (v.isEmpty())
    {
        settings()->remove(ck_ssh_remote_commandline);
        ssh_remote_commandline_ = default_ssh_remote_commandline;
    }
    else if (ssh_remote_commandline_ != v)
    {
        settings()->setValue(ck_ssh_remote_commandline, ssh_remote_commandline_);
        ssh_remote_commandline_ = v;
    }
}
