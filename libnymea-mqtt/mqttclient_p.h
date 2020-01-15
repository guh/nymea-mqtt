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

#ifndef MQTTCLIENT_P_H
#define MQTTCLIENT_P_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QLoggingCategory>

#include "mqttpacket.h"
#include "mqttclient.h"
#include "mqttsubscription.h"

Q_DECLARE_LOGGING_CATEGORY(dbgClient)

class MqttClientPrivate: public QObject
{
    Q_OBJECT
public:
    MqttClientPrivate(MqttClient *q);
    MqttClient *q_ptr;

    void connectToHost(const QString &hostName, quint16 port, bool cleanSession, bool useSsl, const QSslConfiguration &sslConfiguration);
    void disconnectFromHost();

public slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketStateChanged(QAbstractSocket::SocketState socketState);
    void onSocketError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);

    quint16 newPacketId();
    void sendPingreq();
    void restartKeepAliveTimer();

    void reconnectTimerTimeout();

public:
    QString serverHostname;
    quint16 serverPort = 0;
    bool useSsl = false;
    QSslConfiguration sslConfiguration;
    bool autoReconnect = true;
    bool sessionActive = false;
    bool cleanSession = true;
    QSslSocket *socket = nullptr;
    QTimer reconnectTimer;
    int reconnectAttempt = 0;
    quint16 maxReconnectTimeout = 36000;

    QString clientId;
    quint16 keepAlive;
    QTimer keepAliveTimer;
    QString willTopic;
    QByteArray willMessage;
    Mqtt::QoS willQoS = Mqtt::QoS0;
    bool willRetain = false;
    QString username;
    QString password;

    QVector<quint16> unackedPacketList;
    QHash<quint16, MqttPacket> unackedPackets;
};

#endif // MQTTCLIENT_P_H
