/****************************************************************************
**
** Copyright (C) 2019 guh GmbH
** Contact: https://www.nymea.io/licensing/
**
** This file is part of nymea
**
** Commercial License Usage
** Licensees holding valid commercial nymea licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and guh GmbH. For licensing terms
** and conditions see https://www.nymea.io/terms-conditions. For further
** information use the contact form at https://www.nymea.io/imprint.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv3
** included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html
**
**
****************************************************************************/


#ifndef MQTTSERVER_P_H
#define MQTTSERVER_P_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QLoggingCategory>

#include "mqttpacket.h"
#include "mqttserver.h"

Q_DECLARE_LOGGING_CATEGORY(dbgServer)

class ClientContext;
class Subscription;
class SslServer;

class MqttServerPrivate: public QObject
{
    Q_OBJECT
public:
    explicit MqttServerPrivate(MqttServer *q);

    QHash<QString, quint16> publish(const QString &topic, const QByteArray &payload = QByteArray());

public:
    void cleanupClient(QTcpSocket *client);

    void processPacket(const MqttPacket &packet, QTcpSocket *client);
    bool validateTopicFilter(const QString &topicFilter);
    bool matchTopic(const QString &topicFilter, const QString &topic);
    quint16 newPacketId(ClientContext *ctx);

public slots:
    void onClientConnected(QSslSocket *client);
    void onDataAvailable(QSslSocket *client, const QByteArray &data);
    void onClientDisconnected(QSslSocket *client);

public:
    MqttServer *q_ptr;

    QHash<int, SslServer*> servers;
    MqttAuthorizer *authorizer = nullptr;

    Mqtt::QoS maximumSubscriptionQoS = Mqtt::QoS2;

    QHash<QTcpSocket*, QTimer*> pendingConnections;
    QHash<QTcpSocket*, ClientContext*> clientList;
    QHash<QTcpSocket*, QByteArray> clientBuffers;
    QHash<QString, MqttPackets> retainedMessages;
    QHash<QTcpSocket*, SslServer*> clientServerMap;
};

class ClientContext {
public:
    Mqtt::Protocol version = Mqtt::ProtocolUnknown;
    quint16 keepAlive = 0;
    QTimer keepAliveTimer;
    QString clientId;
    QString username;
    QByteArray willTopic;
    QByteArray willMessage;
    Mqtt::QoS willQoS = Mqtt::QoS0;
    bool willRetain = false;

    QByteArray inputBuffer;
    MqttSubscriptions subscriptions;

    QVector<quint16> unackedPacketList;
    QHash<quint16, MqttPacket> unackedPackets;
};

class SslServer: public QTcpServer
{
    Q_OBJECT
public:
    SslServer(const QSslConfiguration &config, QObject *parent = nullptr):
        QTcpServer(parent),
        m_config(config)
    {

    }

signals:
    void clientConnected(QSslSocket *socket);
    void clientDisconnected(QSslSocket *socket);
    void dataAvailable(QSslSocket *socket, const QByteArray &data);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();
    void onSocketReadyRead();

private:
    QSslConfiguration m_config;
};

#endif // MQTTSERVER_P_H
