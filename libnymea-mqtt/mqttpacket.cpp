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

#include "mqttpacket.h"
#include "mqttpacket_p.h"

#include <QDebug>
#include <QDataStream>

Q_LOGGING_CATEGORY(dbgProto, "nymea.mqtt.protocol")

#define ASSERT_LEN(a, name) if (remainingLength < a) { qCWarning(dbgProto) << "Bad" << name << "packet. Data too short."; return -1; }
#define VERIFY_LEN(a, name) if (remainingLength != a) { qCWarning(dbgProto) << "Bad" << name << "packet. Data length unexpected."; return -1; }

MqttPacket::MqttPacket():
    d_ptr(new MqttPacketPrivate(this))
{
}

MqttPacket::MqttPacket(MqttPacket::Type type, quint16 packetId, Mqtt::QoS qos, bool retain, bool dup):
    d_ptr(new MqttPacketPrivate(this))
{
    d_ptr->packetId = packetId;
    d_ptr->header = type;

    switch (type) {
    case TypeConnect:
    case TypeConnack:
    case TypePuback:
    case TypePubrec:
    case TypePubcomp:
    case TypeSuback:
    case TypeUnsuback:
    case TypePingreq:
    case TypePingresp:
    case TypeDisconnect:
        break;
    case TypePublish:
        setDup(dup);
        setQoS(qos);
        setRetain(retain);
        break;
    case TypeSubscribe:
    case TypePubrel:
    case TypeUnsubscribe:
        setDup(false);
        setQoS(Mqtt::QoS1);
        setRetain(false);
        break;
    }
}

MqttPacket::Type MqttPacket::type() const
{
    return static_cast<Type>(d_ptr->header & 0xF0);
}

bool MqttPacket::dup() const
{
    return d_ptr->header & 0x08;
}

void MqttPacket::setDup(bool dup)
{
    if (dup) {
        d_ptr->header |= 0x08;
    } else {
        d_ptr->header &= 0xf7;
    }
}

Mqtt::QoS MqttPacket::qos() const
{
    return static_cast<Mqtt::QoS>((d_ptr->header & 0x06) >> 1);
}

void MqttPacket::setQoS(Mqtt::QoS qoS)
{
    d_ptr->header &= 0xf9;
    d_ptr->header |= (qoS << 1);
}

bool MqttPacket::retain() const
{
    return d_ptr->header & 0x01;
}

void MqttPacket::setRetain(bool retain)
{
    if (retain) {
        d_ptr->header = d_ptr->header | 0x01;
    } else {
        d_ptr->header = d_ptr->header & 0xfe;
    }
}

void MqttPacket::setCleanSession(bool cleanSession)
{
    if (cleanSession) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagCleanSession;
    } else {
        d_ptr->connectFlags &= ~Mqtt::ConnectFlagCleanSession;
    }
}

bool MqttPacket::cleanSession() const
{
    return d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagCleanSession);
}

Mqtt::ConnectFlags MqttPacket::connectFlags() const
{
    return d_ptr->connectFlags;
}

QByteArray MqttPacket::protocolName() const
{
    return d_ptr->protocolName;
}

Mqtt::Protocol MqttPacket::protocolLevel() const
{
    return d_ptr->protocolLevel;
}

void MqttPacket::setProtocolLevel(Mqtt::Protocol protocolLevel)
{
    d_ptr->protocolLevel = protocolLevel;
}

QByteArray MqttPacket::clientId() const
{
    return d_ptr->clientId;
}

void MqttPacket::setClientId(const QByteArray &clientId)
{
    d_ptr->clientId = clientId;
}

QByteArray MqttPacket::willTopic() const
{
    return d_ptr->willTopic;
}

void MqttPacket::setWillTopic(const QByteArray &willTopic)
{
    d_ptr->willTopic = willTopic;
    if (!willTopic.isEmpty()) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagWill;
    } else {
        d_ptr->connectFlags &= ~Mqtt::ConnectFlagWill;
    }
}

QByteArray MqttPacket::willMessage() const
{
    return d_ptr->willMessage;
}

void MqttPacket::setWillMessage(const QByteArray &willMessage)
{
    d_ptr->willMessage = willMessage;
}

Mqtt::QoS MqttPacket::willQoS() const
{
    if (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWillQoS2)) {
        return Mqtt::QoS2;
    }
    if (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWillQoS1)) {
        return Mqtt::QoS1;
    }
    return Mqtt::QoS0;
}

void MqttPacket::setWillQoS(Mqtt::QoS willQoS)
{
    d_ptr->connectFlags &= ~Mqtt::ConnectFlagWillQoS1;
    d_ptr->connectFlags &= ~Mqtt::ConnectFlagWillQoS2;
    if (willQoS == Mqtt::QoS1) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagWillQoS1;
    } else if (willQoS == Mqtt::QoS2) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagWillQoS2;
    }
}

bool MqttPacket::willRetain() const
{
    return d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWillRetain);
}

void MqttPacket::setWillRetain(bool willRetain)
{
    if (willRetain) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagWillRetain;
    } else {
        d_ptr->connectFlags &= ~Mqtt::ConnectFlagWillRetain;
    }
}

QByteArray MqttPacket::username() const
{
    return d_ptr->username;
}

void MqttPacket::setUsername(const QByteArray &username)
{
    d_ptr->username = username;
    if (!username.isEmpty()) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagUsername;
    } else {
        d_ptr->connectFlags &= ~Mqtt::ConnectFlagUsername;
    }
}

QByteArray MqttPacket::password() const
{
    return d_ptr->password;
}

void MqttPacket::setPassword(const QByteArray &password)
{
    d_ptr->password = password;
    if (!password.isEmpty()) {
        d_ptr->connectFlags |= Mqtt::ConnectFlagPassword;
    } else {
        d_ptr->connectFlags &= ~Mqtt::ConnectFlagPassword;
    }
}

quint16 MqttPacket::keepAlive() const
{
    return d_ptr->keepAlive;
}

void MqttPacket::setKeepAlive(quint16 keepAlive)
{
    d_ptr->keepAlive = keepAlive;
}

Mqtt::ConnectReturnCode MqttPacket::connectReturnCode() const
{
    return d_ptr->connectReturnCode;
}

void MqttPacket::setConnectReturnCode(Mqtt::ConnectReturnCode connectReturnCode)
{
    d_ptr->connectReturnCode = connectReturnCode;
}

Mqtt::ConnackFlags MqttPacket::connackFlags() const
{
    return d_ptr->connackFlags;
}

void MqttPacket::setConnackFlags(Mqtt::ConnackFlags connackFlags)
{
    d_ptr->connackFlags = connackFlags;
}

quint16 MqttPacket::packetId() const
{
    return d_ptr->packetId;
}

void MqttPacket::setPacketId(quint16 packetId)
{
    d_ptr->packetId = packetId;
}

QByteArray MqttPacket::topic() const
{
    return d_ptr->topic;
}

void MqttPacket::setTopic(const QByteArray &topic)
{
    d_ptr->topic = topic;
}

QByteArray MqttPacket::payload() const
{
    return d_ptr->payload;
}

void MqttPacket::setPayload(const QByteArray &payload)
{
    d_ptr->payload = payload;
}

MqttSubscriptions MqttPacket::subscriptions() const
{
    return d_ptr->subscriptions;
}

void MqttPacket::setSubscriptions(const MqttSubscriptions &subscriptions)
{
    d_ptr->subscriptions = subscriptions;
}

void MqttPacket::addSubscription(const MqttSubscription &subscription)
{
    d_ptr->subscriptions.append(subscription);
}

Mqtt::SubscribeReturnCodes MqttPacket::subscribeReturnCodes() const
{
    return d_ptr->subscribeReturnCodes;
}

void MqttPacket::setSubscribeReturnCodes(const Mqtt::SubscribeReturnCodes subscribeReturnCodes)
{
    d_ptr->subscribeReturnCodes = subscribeReturnCodes;
}

void MqttPacket::addSubscribeReturnCode(Mqtt::SubscribeReturnCode subscribeReturnCode)
{
    d_ptr->subscribeReturnCodes.append(subscribeReturnCode);
}

int MqttPacket::parse(const QByteArray &buffer)
{
    if (buffer.length() < 2) {
        return 0;
    }
    QDataStream inputStream(buffer);
//    qCDebug(dbgProto()) << "MQTT input data:\n" << buffer.toHex();

    inputStream >> d_ptr->header;

    quint16 remainingLength = 0;
    int multiplier = 1;
    quint8 lengthBit;
    quint8 lenFields = 0;
    do {
        inputStream >> lengthBit;
        remainingLength += (lengthBit & 0x7F) * multiplier;
        multiplier *= 128;
        lenFields++;
        if (multiplier > 128*128*128) {
            qCWarning(dbgProto) << "Remaining Length field invalid";
            return -1;
        }
    } while((lengthBit & 0x80) != 0);

    if (remainingLength > buffer.length() - 1 - lenFields) {
        qCDebug(dbgProto) << "Cannot process MQTT packet. Remaining Length field larger than input data size:" << remainingLength << ">" << (buffer.length() - 1 - lenFields);
        return 0;
    }

    if (!d_ptr->verifyHeaderFlags()) {
        qCDebug(dbgProto) << "Bad MQTT packet. Fixed header flags invalid.";
        return -1;
    }

    const quint16 fullRemainingLength = remainingLength;

    quint16 strLen;
    const quint16 MAX_STRLEN = remainingLength;
    char str[MAX_STRLEN];

    switch (type()) {
    case TypeConnect: {
        ASSERT_LEN(2, "CONNECT");
        inputStream >> strLen;
        remainingLength -= 2;

        ASSERT_LEN(strLen, "CONNECT");
        memset(str, 0, MAX_STRLEN);
        inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
        remainingLength -= strLen;
        d_ptr->protocolName = QByteArray(str);

        ASSERT_LEN(6, "CONNECT");
        quint8 pl;
        inputStream >> pl;
        d_ptr->protocolLevel = static_cast<Mqtt::Protocol>(pl);
        remainingLength -= 1;
        quint8 cF;
        inputStream >> cF;
        remainingLength -= 1;
        d_ptr->connectFlags = static_cast<Mqtt::ConnectFlags>(cF);
        inputStream >> d_ptr->keepAlive;
        remainingLength -= 2;

        inputStream >> strLen;
        remainingLength -= 2;

        ASSERT_LEN(strLen, "CONNECT");
        memset(str, 0, MAX_STRLEN);
        inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
        remainingLength -= strLen;
        d_ptr->clientId = QByteArray(str);

        if (connectFlags().testFlag(Mqtt::ConnectFlagWill)) {
            ASSERT_LEN(2, "CONNECT");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "CONNECT");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            d_ptr->willTopic = QByteArray(str);

            ASSERT_LEN(2, "CONNECT");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "CONNECT");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            d_ptr->willMessage = QByteArray(str);
        } else {
            if (willRetain() || willQoS() != Mqtt::QoS0) {
                qCWarning(dbgProto) << "Bad CONNECT packet. Will flag not set but WillQoS or WillRetain set.";
                return -1;
            }
        }

        if (connectFlags().testFlag(Mqtt::ConnectFlagUsername)) {
            ASSERT_LEN(2, "CONNECT");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "CONNECT");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            d_ptr->username = QByteArray(str);
        } else {
            if (connectFlags().testFlag(Mqtt::ConnectFlagPassword)) {
                qCWarning(dbgProto) << "Bad CONNECT packet. Username flag not set but password is set.";
                return -1;
            }
        }

        if (connectFlags().testFlag(Mqtt::ConnectFlagPassword)) {
            ASSERT_LEN(2, "CONNECT");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "CONNECT");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            d_ptr->password = QByteArray(str);
        }
        VERIFY_LEN(0, "CONNECT");
        break;
    }
    case TypeConnack: {
        VERIFY_LEN(2, "CONNACK");
        quint8 connackFlags;
        inputStream >> connackFlags;
        remainingLength -= 1;
        d_ptr->connackFlags = static_cast<Mqtt::ConnackFlags>(connackFlags);
        quint8 connectReturnCode;
        inputStream >> connectReturnCode;
        d_ptr->connectReturnCode = static_cast<Mqtt::ConnectReturnCode>(connectReturnCode);
        remainingLength -= 1;
        VERIFY_LEN(0, "CONNACK");
        break;
    }
    case TypePublish: {
        ASSERT_LEN(2, "PUBLISH");
        inputStream >> strLen;
        remainingLength -= 2;
        ASSERT_LEN(strLen, "PUBLISH");
        memset(str, 0, MAX_STRLEN);
        inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
        remainingLength -= strLen;
        d_ptr->topic = QByteArray(str);

        if (qos() == Mqtt::QoS1 || qos() == Mqtt::QoS2) {
            ASSERT_LEN(2, "PUBLISH");
            inputStream >> d_ptr->packetId;
            remainingLength -= 2;
        }

        memset(str, 0, MAX_STRLEN);
        inputStream.readRawData(str, qMin(MAX_STRLEN, remainingLength));
        d_ptr->payload = QByteArray(str);
        break;
    }
    case TypePuback:
        VERIFY_LEN(2, "PUBACK");
        inputStream >> d_ptr->packetId;
        break;
    case TypePubrec:
        VERIFY_LEN(2, "PUBREC");
        inputStream >> d_ptr->packetId;
        break;
    case TypePubrel:
        VERIFY_LEN(2, "PUBREL");
        inputStream >> d_ptr->packetId;
        break;
    case TypePubcomp:
        VERIFY_LEN(2, "PUBCOMP");
        inputStream >> d_ptr->packetId;
        break;
    case TypeSubscribe: {
        ASSERT_LEN(2, "SUBSCRIBE");
        inputStream >> d_ptr->packetId;
        remainingLength -= 2;

        if (remainingLength == 0) {
            qCWarning(dbgProto) << "Bad SUBSCRIBE packet. Subscription filter in payload missing.";
            return -1;
        }
        while (remainingLength > 0) {
            ASSERT_LEN(2, "SUBSCRIBE");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "SUBSCRIBE");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            MqttSubscription subscription;
            subscription.setTopicFilter(QByteArray(str));

            ASSERT_LEN(1, "SUBSCRIBE");
            quint8 requestedQoS;
            inputStream >> requestedQoS;
            remainingLength -= 1;
            if ((requestedQoS & 0xFC) != 0x00) {
                qCWarning(dbgProto) << "Bad SUBSCRIBE packet. Reserved bits set in requested QoS field.";
                return -1;
            }
            if ((requestedQoS & 0x03) == 0x03) {
                qCWarning(dbgProto) << "Bad SUBSCRIBE packet. QoS cannot be QoS1 and QoS2 at the same time.";
                return -1;
            }
            subscription.setQoS(static_cast<Mqtt::QoS>(requestedQoS));
            d_ptr->subscriptions.append(subscription);
        }
        break;
    }
    case TypeSuback:
        ASSERT_LEN(3, "SUBACK");
        inputStream >> d_ptr->packetId;
        remainingLength -= 2;
        while (remainingLength > 0) {
            quint8 subscribeReturnCode;
            inputStream >> subscribeReturnCode;
            remainingLength -= 1;
            d_ptr->subscribeReturnCodes.append(static_cast<Mqtt::SubscribeReturnCode>(subscribeReturnCode));
        }
        break;
    case TypeUnsubscribe: {
        ASSERT_LEN(5, "UNSUBSCRIBE");
        inputStream >> d_ptr->packetId;
        remainingLength -= 2;
        while (remainingLength > 0) {
            ASSERT_LEN(2, "UNSUBSCRIBE");
            inputStream >> strLen;
            remainingLength -= 2;
            ASSERT_LEN(strLen, "UNSUBSCRIBE");
            memset(str, 0, MAX_STRLEN);
            inputStream.readRawData(str, qMin(MAX_STRLEN, strLen));
            remainingLength -= strLen;
            MqttSubscription subscription;
            subscription.setTopicFilter(QByteArray(str));
            d_ptr->subscriptions.append(subscription);
        }
        }
        break;
    case TypeUnsuback:
        VERIFY_LEN(2, "UNSUBACK");
        inputStream >> d_ptr->packetId;
        break;
    case TypePingreq:
        VERIFY_LEN(0, "PINGREC");
        break;
    case TypePingresp:
        VERIFY_LEN(0, "PINGRESP");
        break;
    case TypeDisconnect:
        VERIFY_LEN(0, "DISCONNECT");
        break;
    }
    return fullRemainingLength + 1 + lenFields;
}

QByteArray MqttPacket::serialize() const
{
    QByteArray ret;
    QDataStream stream(&ret, QIODevice::WriteOnly);
    stream << d_ptr->header;

    quint16 remainingLength = 0;
    switch (type()) {
    case TypeConnect:
        remainingLength = static_cast<quint16>(
                    2 // protocol name length
                    + d_ptr->protocolName.length()
                    + 1 // protocol level
                    + 1 // connect flags
                    + 2 // keep alive
                    + 2 // client id length
                    + d_ptr->clientId.length()
                    + (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWill) ? (2 + d_ptr->willTopic.length()) : 0)
                    + (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWill) ? (2 + d_ptr->willMessage.length()) : 0)
                    + (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagUsername) ? (2 + d_ptr->username.length()) : 0)
                    + (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagPassword) ? (2 + d_ptr->password.length()) : 0)
                );
        break;
    case TypeConnack:
        remainingLength = 2;
        break;
    case TypePublish:
        remainingLength += 2; // len for topic
        remainingLength += d_ptr->topic.length();
        if (qos() == Mqtt::QoS1 || qos() == Mqtt::QoS2) {
            remainingLength += 2; // packetId
        }
        remainingLength += d_ptr->payload.length();
        break;
    case TypePuback:
    case TypePubrec:
    case TypePubrel:
    case TypePubcomp:
        remainingLength = 2;
        break;
    case TypeSubscribe:
        remainingLength = 2; // packet id
        foreach (const MqttSubscription &subscription, d_ptr->subscriptions) {
            remainingLength += 2; // topic filter length
            remainingLength += static_cast<quint16>(subscription.topicFilter().length());
            remainingLength += 1; // requested QoS
        }
        break;
    case TypeSuback:
        remainingLength = 2 + static_cast<quint16>(d_ptr->subscribeReturnCodes.length());
        break;
    case TypeUnsubscribe:
        remainingLength = 2; // packet id
        foreach (const MqttSubscription &subscription, d_ptr->subscriptions) {
            remainingLength += 2;
            remainingLength += static_cast<quint16>(subscription.topicFilter().length());
        }
        break;
    case TypeUnsuback:
        remainingLength = 2; // packet id
        break;
    case TypePingreq:
        break;
    case TypePingresp:
        break;
    case TypeDisconnect:
        break;
    }

    quint8 encodedByte;
    do {
        encodedByte = remainingLength % 128;
        remainingLength /= 128;
        if ( remainingLength > 0 ) {
            encodedByte = encodedByte | 128;
        }
        stream << encodedByte;
    } while ( remainingLength > 0 );

    switch (type()) {
    case TypeConnect:
        stream << static_cast<quint16>(d_ptr->protocolName.length());
        stream.writeRawData(d_ptr->protocolName.data(), d_ptr->protocolName.length());
        stream << static_cast<quint8>(d_ptr->protocolLevel);
        stream << static_cast<quint8>(d_ptr->connectFlags);
        stream << static_cast<quint16>(d_ptr->keepAlive);
        stream << static_cast<quint16>(d_ptr->clientId.length());
        stream.writeRawData(d_ptr->clientId.data(), d_ptr->clientId.length());
        if (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagWill)) {
            stream << static_cast<quint16>(d_ptr->willTopic.length());
            stream.writeRawData(d_ptr->willTopic.data(), d_ptr->willTopic.length());
            stream << static_cast<quint16>(d_ptr->willMessage.length());
            stream.writeRawData(d_ptr->willMessage.data(), d_ptr->willMessage.length());
        }
        if (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagUsername)) {
            stream << static_cast<quint16>(d_ptr->username.length());
            stream.writeRawData(d_ptr->username.data(), d_ptr->username.length());
        }
        if (d_ptr->connectFlags.testFlag(Mqtt::ConnectFlagPassword)) {
            stream << static_cast<quint16>(d_ptr->password.length());
            stream.writeRawData(d_ptr->password.data(), d_ptr->password.length());
        }
        break;
    case TypeConnack:
        stream << static_cast<quint8>(d_ptr->connackFlags);
        stream << static_cast<quint8>(d_ptr->connectReturnCode);
        break;
    case TypePublish:
        stream << static_cast<quint16>(d_ptr->topic.length());
        stream.writeRawData(d_ptr->topic.data(), d_ptr->topic.length());
        if (qos() == Mqtt::QoS1 || qos() == Mqtt::QoS2) {
            stream << d_ptr->packetId;
        }
        stream.writeRawData(d_ptr->payload.data(), d_ptr->payload.length());
        break;
    case TypePuback:
    case TypePubrec:
    case TypePubrel:
    case TypePubcomp:
        stream << d_ptr->packetId;
        break;
    case TypeSubscribe:
        stream << static_cast<quint16>(d_ptr->packetId);
        foreach (const MqttSubscription &subscription, d_ptr->subscriptions) {
            stream << static_cast<quint16>(subscription.topicFilter().length());
            stream.writeRawData(subscription.topicFilter().data(), subscription.topicFilter().length());
            stream << static_cast<quint8>(subscription.qoS());
        }
        break;
    case TypeSuback:
        stream << d_ptr->packetId;
        foreach (Mqtt::SubscribeReturnCode subscribeReturnCode, d_ptr->subscribeReturnCodes) {
            stream << static_cast<quint8>(subscribeReturnCode);
        }
        break;
    case TypeUnsubscribe:
        stream << d_ptr->packetId;
        foreach (const MqttSubscription &subscription, d_ptr->subscriptions) {
            stream << static_cast<quint16>(subscription.topicFilter().length());
            stream.writeRawData(subscription.topicFilter().data(), subscription.topicFilter().length());
        }
        break;
    case TypeUnsuback:
        stream << d_ptr->packetId;
        break;
    case TypePingreq:
        break;
    case TypePingresp:
        break;
    case TypeDisconnect:
        break;
    }
//    qCDebug(dbgProto()) << "Serialized MQTT packet:" << ret.toHex();
    return ret;
}

bool MqttPacket::operator==(const MqttPacket &other) const
{
    return serialize() == other.serialize();
}

bool MqttPacketPrivate::verifyHeaderFlags()
{
    bool fail = false;

    switch (q_ptr->type()) {
    case MqttPacket::MqttPacket::TypeConnect:
    case MqttPacket::TypeConnack:
    case MqttPacket::TypePuback:
    case MqttPacket::TypePubrec:
    case MqttPacket::TypePubcomp:
    case MqttPacket::TypeSuback:
    case MqttPacket::TypeUnsuback:
    case MqttPacket::TypePingreq:
    case MqttPacket::TypePingresp:
    case MqttPacket::TypeDisconnect:
        fail |= q_ptr->dup();
        fail |= (q_ptr->qos() != Mqtt::QoS0);
        fail |= q_ptr->retain();
        break;
    case MqttPacket::TypePublish:
        fail |= (q_ptr->qos() == 0x03);
        break;
    case MqttPacket::TypeSubscribe:
    case MqttPacket::TypePubrel:
    case MqttPacket::TypeUnsubscribe:
        fail |= q_ptr->dup();
        fail |= (q_ptr->qos() != Mqtt::QoS1);
        fail |= q_ptr->retain();
        break;
    }
    return !fail;
}
