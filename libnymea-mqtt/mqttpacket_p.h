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

#ifndef MQTTPACKET_P_H
#define MQTTPACKET_P_H

#include <QByteArray>
#include <QByteArray>
#include <QList>
#include <QLoggingCategory>

#include "mqtt.h"
#include "mqttpacket.h"
#include "mqttsubscription.h"

Q_DECLARE_LOGGING_CATEGORY(dbgProto)

class MqttPacketPrivate
{
public:
    MqttPacketPrivate(MqttPacket *q) : q_ptr(q) { }
    MqttPacket *q_ptr;

    bool verifyHeaderFlags();

    quint8 header = 0;
    QByteArray protocolName = "MQTT";
    Mqtt::Protocol protocolLevel = Mqtt::ProtocolUnknown;
    Mqtt::ConnectFlags connectFlags = Mqtt::ConnectFlagNone;
    Mqtt::ConnackFlags connackFlags = Mqtt::ConnackFlagNone;
    quint16 keepAlive = 0;
    QByteArray clientId;
    QByteArray willTopic;
    QByteArray willMessage;
    QByteArray username;
    QByteArray password;

    quint16 packetId = 0;
    QByteArray topic;
    QByteArray payload;

    Mqtt::ConnectReturnCode connectReturnCode = Mqtt::ConnectReturnCodeAccepted;

    MqttSubscriptions subscriptions;
    Mqtt::SubscribeReturnCodes subscribeReturnCodes;
};

#endif // MQTTPACKET_P_H
