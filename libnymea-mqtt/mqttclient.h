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

#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QSslConfiguration>

#include "mqttpacket.h"
#include "mqttsubscription.h"

class MqttClientPrivate;

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(const QString &clientId, QObject *parent = nullptr);
    explicit MqttClient(const QString &clientId, quint16 keepAlive = 300, const QString &willTopic = QString(), const QByteArray &willMessage = QByteArray(), Mqtt::QoS willQoS = Mqtt::QoS0, bool willRetain = false, QObject *parent = nullptr);

    bool autoReconnect() const;
    void setAutoReconnect(bool autoReconnect);

    quint16 maxAutoReconnectTimeout() const;
    void setMaxAutoReconnectTimeout(quint16 maxAutoReconnectTimeout);

    quint16 keepAlive() const;
    void setKeepAlive(quint16 keepAlive);

    QString willTopic() const;
    void setWillTopic(const QString &willTopic);

    QByteArray willMessage() const;
    void setWillMessage(const QByteArray &willMessage);

    Mqtt::QoS willQoS() const;
    void setWillQoS(Mqtt::QoS willQoS);

    bool willRetain() const;
    void setWillRetain(bool willRetain);

    QString username() const;
    void setUsername(const QString &username);

    QString password() const;
    void setPassword(const QString &password);

    void connectToHost(const QString &hostName, quint16 port, bool cleanSession = true, bool useSsl = false, const QSslConfiguration &sslConfiguration = QSslConfiguration());
    void disconnectFromHost();

    bool isConnected() const;

public slots:
    quint16 subscribe(const MqttSubscription &subscription);
    quint16 subscribe(const QString &topciFilter, Mqtt::QoS qos = Mqtt::QoS0);
    quint16 subscribe(const MqttSubscriptions &subscriptions);

    quint16 unsubscribe(const MqttSubscription &subscription);
    quint16 unsubscribe(const QString &topicFilter);
    quint16 unsubscribe(const MqttSubscriptions &subscriptions);

    quint16 publish(const QString &topic, const QByteArray &payload, Mqtt::QoS qos = Mqtt::QoS0, bool retain = false);

signals:
    void connected(Mqtt::ConnectReturnCode connectReturnCode, Mqtt::ConnackFlags connackFlags);
    void disconnected();
    void stateChanged(QAbstractSocket::SocketState state);
    void error(QAbstractSocket::SocketError socketError);

    void subscribeResult(quint16 packetId, const Mqtt::SubscribeReturnCodes &subscribeReturnCodes);
    void subscribed(const QString &topic, Mqtt::SubscribeReturnCode subscribeReturnCode);
    void unsubscribed(quint16 packetId);
    void published(quint16 packetId, const QString &topic);
    void publishReceived(const QString &topic, const QByteArray &payload, bool retained);

private:
    MqttClientPrivate *d_ptr;
    friend class OperationTests;
};

#endif // MQTTCLIENT_H
