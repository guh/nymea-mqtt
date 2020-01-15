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


#ifndef MQTTSERVER_H
#define MQTTSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QLoggingCategory>
#include <QSslConfiguration>

#include "mqttpacket.h"

class MqttServerPrivate;
class Subscription;

class MqttAuthorizer {
public:
    virtual ~MqttAuthorizer() = default;
    virtual Mqtt::ConnectReturnCode authorizeConnect(int serverAddressId, const QString &clientId, const QString &username, const QString &password, const QHostAddress &peerAddress) = 0;
    virtual bool authorizeSubscribe(int serverAddressId, const QString &clientId, const QString &topicFilter) = 0;
    virtual bool authorizePublish(int serverAddressId, const QString &clientId, const QString &topic) = 0;
};

class MqttServer : public QObject
{
    Q_OBJECT
public:
    explicit MqttServer(QObject *parent = nullptr);

    Mqtt::QoS maximumSubscriptionsQoS() const;
    void setMaximumSubscriptionsQoS(Mqtt::QoS maximumSubscriptionQoS);

    void setAuthorizer(MqttAuthorizer *authorizer);

    int listen(const QHostAddress &address = QHostAddress::Any, quint16 port = 1883, const QSslConfiguration &sslConfiguration = QSslConfiguration());
    QList<int> listeningAddressIds() const;
    QPair<QHostAddress, quint16> listeningAddress(int addressId);
    void close(int addressId);
    bool isListening(const QHostAddress &address, quint16 port) const;

    QStringList clients() const;
    void disconnectClient(const QString &clientId);

    // allows publishing from the server, including topcis starting with $
    QHash<QString, quint16> publish(const QString &topic, const QByteArray &payload = QByteArray());

signals:
    // emitted whenever a client connects, after the mqtt connect handshake has been done.
    void clientConnected(int serverAddressId, const QString &clientId, const QString &username, const QHostAddress &clientAddress);
    // emitted whenever a client disconnects, that is, when a DISCONNECT message has been received or the keep alive timeout has been reached.
    void clientDisconnected(const QString &clientId);
    // emitted whenever a client has been seen, that is, a control message or a keep alive message has been received.
    void clientAlive(const QString &clientId);
    // emitted whenever a client subscribes, a client can also subscribe to topics starting with $ but those won't be relayed from other clients. Only internal server publishes to $ topcis will be sent to subscribed clients.
    void clientSubscribed(const QString &clientId, const QString &topicFilter, Mqtt::QoS requestedQoS);
    // emitted whenever a client unsubscribes from a topic
    void clientUnsubscribed(const QString &clientId, const QString &topicFiltr);
    // emitted whenever a publish message is received from a client before the message is relayed to other clients. Topics starting with $ will be received here, but not relayed to other clients.
    void publishReceived(const QString &clientId, quint16 packetId, const QString &topic, const QByteArray &payload);
    // emitted whenever a publish message is sent to a client. Note: this might be fired often if many clients are connected and subsribed to matching topic filters.
    void published(const QString &clientId, quint16 packetId, const QString &topic, const QByteArray &payload);

private:
    MqttServerPrivate *d_ptr;
};

#endif // MQTTSERVER_H
