// Copyright (c) 2017-2025 Manuel Schneider

#include "plugin.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <QWidget>
#include <ranges>
#include <albert/albert.h>
#include <albert/logging.h>
#include <albert/standarditem.h>
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

Plugin::Plugin():
    tr_desc(tr("Configured SSH host – %1")),
    tr_conn(tr("Connect"))
{
    hosts.unite(parseConfigFile(u"/etc/ssh/config"_s));
    hosts.unite(parseConfigFile(QDir::home().filePath(u".ssh/config"_s)));
    INFO << u"Found %1 ssh hosts."_s.arg(hosts.size());
}

QString Plugin::synopsis(const QString &) const
{ return tr("[user@]<host> [params…]"); }

bool Plugin::allowTriggerRemap() const
{ return false; }

std::vector<RankItem> Plugin::getItems(const QString &query, bool allowParams) const
{
    vector<RankItem> r;

    static const auto regex_synopsis =
        QRegularExpression(uR"(^(?:(\w+)@)?\[?([\w\.-]*)\]?(?:\h+(.*))?$)"_s);

    auto match = regex_synopsis.match(query);
    if (!match.hasMatch())
        return r;

    const auto q_user = match.captured(1);
    const auto q_host = match.captured(2);
    const auto q_params = match.captured(3);

    if (!(allowParams || q_params.isEmpty()))
        return r;

    for (const auto &host : hosts)
    {
        if (host.startsWith(q_host, Qt::CaseInsensitive))
        {
            QString cmd = defaultTrigger();

            if (!q_user.isEmpty())
                cmd += q_user + u'@';
            cmd += host;
            if (!q_params.isEmpty())
                cmd += u' ' + q_params;

            auto a = [cmd, this]{ apps->runTerminal(u"%1 || exec $SHELL"_s.arg(cmd)); };

            r.emplace_back(
                StandardItem::make(host,
                                   host,
                                   tr_desc.arg(cmd),
                                   icon_urls,
                                   {{u"c"_s, tr_conn, a}},
                                   cmd.mid(defaultTrigger().length())),
                (double)q_host.size() / host.size()
            );
        }
    }

    return r;
}

void Plugin::handleTriggerQuery(Query &query)
{
    auto r = getItems(query.string(), true);
    applyUsageScore(&r);
    ranges::sort(r, ::greater());
    auto v = r | views::transform(&RankItem::item);
    query.add({v.begin(), v.end()});
}

vector<RankItem> Plugin::handleGlobalQuery(const Query &query)
{
    if (query.string().trimmed().isEmpty())
        return {};
    return getItems(query.string(), false);
}

QWidget *Plugin::buildConfigWidget()
{
    auto *w = new QLabel(tr(
        "Provides session launch action items for host patterns in the "
        "SSH config that do not contain globbing characters."
    ));
    w->setAlignment(Qt::AlignTop);
    w->setWordWrap(true);
    return w;
}
