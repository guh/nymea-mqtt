#include "mqttclient.h"
#include "mqttclient_p.h"
#include "mqttpacket.h"

Q_LOGGING_CATEGORY(dbgClient, "nymea.mqtt.client")

MqttClientPrivate::MqttClientPrivate(MqttClient *q):
    QObject(q),
    q_ptr(q)
{
    qRegisterMetaType<Mqtt::SubscribeReturnCodes>();
    qRegisterMetaType<Mqtt::ConnackFlags>();
}

void MqttClientPrivate::connectToHost(const QString &hostName, quint16 port, bool cleanSession)
{
    serverHostname = hostName;
    serverPort = port;
    this->cleanSession = cleanSession;

    sessionActive = true;

    if (socket) {
        socket->abort();
        socket->deleteLater();
    }
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, this, &MqttClientPrivate::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &MqttClientPrivate::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &MqttClientPrivate::onReadyRead);
    connect(socket, &QTcpSocket::stateChanged, this, &MqttClientPrivate::onSocketStateChanged);
//    connect(d_ptr->socket, &QTcpSocket::error, this, &MqttClient::error);
    socket->connectToHost(hostName, port);
}

void MqttClientPrivate::disconnectFromHost()
{
    sessionActive = false;
    if (!socket || !socket->isOpen()) {
        return;
    }
    MqttPacket packet(MqttPacket::TypeDisconnect);
    socket->write(packet.serialize());
    socket->flush();
    socket->disconnectFromHost();
}

MqttClient::MqttClient(const QString &clientId, QObject *parent):
    QObject(parent),
    d_ptr(new MqttClientPrivate(this))
{
    d_ptr->clientId = clientId;

}

MqttClient::MqttClient(const QString &clientId, quint16 keepAlive, const QString &willTopic, const QByteArray &willMessage, Mqtt::QoS willQoS, bool willRetain, QObject *parent):
    QObject(parent),
    d_ptr(new MqttClientPrivate(this))
{

    d_ptr->clientId = clientId;
    d_ptr->keepAlive = keepAlive;
    d_ptr->willTopic = willTopic;
    d_ptr->willMessage = willMessage;
    d_ptr->willQoS = willQoS;
    d_ptr->willRetain = willRetain;

    if (keepAlive > 0) {
        connect(&d_ptr->keepAliveTimer, &QTimer::timeout, d_ptr, &MqttClientPrivate::sendPingreq);
    }
}

bool MqttClient::autoReconnect() const
{
    return d_ptr->autoReconnect;
}

void MqttClient::setAutoReconnect(bool autoReconnect)
{
    d_ptr->autoReconnect = autoReconnect;
}

void MqttClient::setKeepAlive(quint16 keepAlive)
{
    d_ptr->keepAlive = keepAlive;
}

QString MqttClient::willTopic() const
{
    return d_ptr->willTopic;
}

void MqttClient::setWillTopic(const QString &willTopic)
{
    d_ptr->willTopic = willTopic;
}

QByteArray MqttClient::willMessage() const
{
    return d_ptr->willMessage;
}

void MqttClient::setWillMessage(const QByteArray &willMessage)
{
    d_ptr->willMessage = willMessage;
}

Mqtt::QoS MqttClient::willQoS() const
{
    return d_ptr->willQoS;
}

void MqttClient::setWillQoS(Mqtt::QoS willQoS)
{
    d_ptr->willQoS = willQoS;
}

bool MqttClient::willRetain() const
{
    return d_ptr->willRetain;
}

void MqttClient::setWillRetain(bool willRetain)
{
    d_ptr->willRetain = willRetain;
}

QString MqttClient::username() const
{
    return d_ptr->username;
}

void MqttClient::setUsername(const QString &username)
{
    d_ptr->username = username;
}

QString MqttClient::password() const
{
    return d_ptr->password;
}

void MqttClient::setPassword(const QString &password)
{
    d_ptr->password = password;
}

void MqttClient::connectToHost(const QString &hostName, quint16 port, bool cleanSession)
{
    d_ptr->connectToHost(hostName, port, cleanSession);
}

void MqttClient::disconnectFromHost()
{
    d_ptr->disconnectFromHost();
}

bool MqttClient::isConnected() const
{
    return d_ptr->socket && d_ptr->socket->state() == QAbstractSocket::ConnectedState && d_ptr->keepAliveTimer.isActive();
}

quint16 MqttClient::subscribe(const MqttSubscription &subscription)
{
    MqttSubscriptions subscriptions = {subscription};
    return subscribe(subscriptions);
}

quint16 MqttClient::subscribe(const QString &topicFilter, Mqtt::QoS qos)
{
    MqttSubscription subscription(topicFilter.toUtf8(), qos);
    return subscribe(subscription);
}

quint16 MqttClient::subscribe(const MqttSubscriptions &subscriptions)
{
    MqttPacket packet(MqttPacket::TypeSubscribe, d_ptr->newPacketId());
    packet.setSubscriptions(subscriptions);
    d_ptr->unackedPackets.insert(packet.packetId(), packet);
    d_ptr->unackedPacketList.append(packet.packetId());
    d_ptr->socket->write(packet.serialize());
    return packet.packetId();
}

quint16 MqttClient::unsubscribe(const MqttSubscription &subscription)
{
    MqttSubscriptions subscriptions = {subscription};
    return unsubscribe(subscriptions);
}

quint16 MqttClient::unsubscribe(const QString &topicFilter)
{
    return unsubscribe(MqttSubscription(topicFilter.toUtf8(), Mqtt::QoS0));
}

quint16 MqttClient::unsubscribe(const MqttSubscriptions &subscriptions)
{
    MqttPacket packet(MqttPacket::TypeUnsubscribe, d_ptr->newPacketId());
    packet.setSubscriptions(subscriptions);
    d_ptr->unackedPackets.insert(packet.packetId(), packet);
    d_ptr->unackedPacketList.append(packet.packetId());
    d_ptr->socket->write(packet.serialize());
    return packet.packetId();
}

quint16 MqttClient::publish(const QString &topic, const QByteArray &payload, Mqtt::QoS qos, bool retain)
{
    quint16 packetId = qos >= Mqtt::QoS1 ? d_ptr->newPacketId() : 0;
    MqttPacket packet(MqttPacket::TypePublish, packetId, qos, retain, false);
    packet.setTopic(topic.toUtf8());
    packet.setPayload(payload);
    d_ptr->socket->write(packet.serialize());
    if (qos == Mqtt::QoS0) {
        QTimer::singleShot(0, this, [this, packetId](){
            emit published(packetId);
        });
    } else {
        d_ptr->unackedPackets.insert(packet.packetId(), packet);
        d_ptr->unackedPacketList.append(packetId);
    }
    return packetId;
}

void MqttClientPrivate::onConnected()
{
    MqttPacket packet(MqttPacket::TypeConnect);
    packet.setProtocolLevel(Mqtt::Protocol311);
    packet.setCleanSession(cleanSession);
    packet.setKeepAlive(keepAlive);
    packet.setClientId(clientId.toUtf8());
    packet.setWillTopic(willTopic.toUtf8());
    packet.setWillMessage(willMessage);
    packet.setWillQoS(willQoS);
    packet.setWillRetain(willRetain);
    packet.setUsername(username.toUtf8());
    packet.setPassword(password.toUtf8());
    socket->write(packet.serialize());
}

void MqttClientPrivate::onDisconnected()
{
    qCDebug(dbgClient) << "Disconnected from server";
    emit q_ptr->disconnected();
    if (sessionActive && autoReconnect) {
        connectToHost(serverHostname, serverPort, cleanSession);
    }
}

void MqttClientPrivate::onReadyRead()
{
    static QByteArray data;
    data.append(socket->readAll());
//    qCDebug(dbgClient) << "Received data from server:" << data.toHex();
    MqttPacket packet;
    int ret = packet.parse(data);
    if (ret == -1) {
        qCDebug(dbgClient) << "Bad data from server. Dropping connection.";
        data.clear();
        socket->abort();
        return;
    }
    if (ret == 0) {
        qCDebug(dbgClient) << "Not enough data from server...";
        return;
    }
    data.remove(0, ret);

    switch (packet.type()) {
    case MqttPacket::TypeConnack:
        emit q_ptr->connected(packet.connectReturnCode(), packet.connackFlags());
        if (packet.connectReturnCode() != Mqtt::ConnectReturnCodeAccepted) {
            qCWarning(dbgClient) << "MQTT connection refused:" << packet.connectReturnCode();
            socket->abort();
            emit q_ptr->disconnected();
            return;
        }
        foreach (quint16 retryPacketId, unackedPacketList) {
            MqttPacket retryPacket = unackedPackets.value(retryPacketId);
            retryPacket.setDup(true);
            socket->write(retryPacket.serialize());
        }
        restartKeepAliveTimer();
        break;
    case MqttPacket::TypePublish:
        qCDebug(dbgClient) << "Publish received from server. Topic:" << packet.topic() << "Payload:" << packet.payload() << "QoS:" << packet.qos();
        switch (packet.qos()) {
        case Mqtt::QoS0:
            emit q_ptr->publishReceived(packet.topic(), packet.payload(), packet.retain());
            break;
        case Mqtt::QoS1: {
            emit q_ptr->publishReceived(packet.topic(), packet.payload(), packet.retain());
            MqttPacket response(MqttPacket::TypePuback, packet.packetId());
            socket->write(response.serialize());
            break;
        }
        case Mqtt::QoS2: {
            if (!packet.dup() && unackedPacketList.contains(packet.packetId())) {
                // Hmm... Server says it's not a duplicate, but packet id is not released yet... Drop connection.
                socket->disconnectFromHost();
                return;
            }

            MqttPacket response(MqttPacket::TypePubrec, packet.packetId());

            if (!unackedPacketList.contains(packet.packetId())) {
                unackedPackets.insert(packet.packetId(), response);
                unackedPacketList.append(packet.packetId());
                emit q_ptr->publishReceived(packet.topic(), packet.payload(), packet.retain());
            }
            socket->write(response.serialize());
            break;
        }
        }
        break;
    case MqttPacket::TypePuback:
        unackedPackets.remove(packet.packetId());
        unackedPacketList.removeAll(packet.packetId());
        emit q_ptr->published(packet.packetId());
        restartKeepAliveTimer();
        break;
    case MqttPacket::TypePubrec: {
        MqttPacket response(MqttPacket::TypePubrel, packet.packetId());
        unackedPackets[packet.packetId()] = response;
        socket->write(response.serialize());
        restartKeepAliveTimer();
        break;
    }
    case MqttPacket::TypePubrel: {
        MqttPacket response(MqttPacket::TypePubcomp, packet.packetId());
        unackedPackets[packet.packetId()] = response;
        socket->write(response.serialize());
        restartKeepAliveTimer();
        break;
    }
    case MqttPacket::TypePubcomp:
        unackedPackets.remove(packet.packetId());
        unackedPacketList.removeAll(packet.packetId());
        emit q_ptr->published(packet.packetId());
        restartKeepAliveTimer();
        break;
    case MqttPacket::TypeSuback:
        unackedPackets.remove(packet.packetId());
        unackedPacketList.removeAll(packet.packetId());
        emit q_ptr->subscribed(packet.packetId(), packet.subscribeReturnCodes());
        restartKeepAliveTimer();
        break;
    case MqttPacket::TypeUnsuback:
        if (!unackedPackets.contains(packet.packetId())) {
            qCWarning(dbgClient) << "UNSUBACK received but not waiting for it. Dropping connection. Packet ID:" << packet.packetId();
            socket->abort();
            return;
        }
        unackedPackets.remove(packet.packetId());
        unackedPacketList.removeAll(packet.packetId());
        emit q_ptr->unsubscribed(packet.packetId());
        restartKeepAliveTimer();
        break;
    case MqttPacket::TypePingresp:
        break;
    default:
        qCDebug(dbgClient).noquote().nospace() << "Unhandled packet type: 0x" << QString::number(packet.type(), 16);
        Q_ASSERT(false);
    }

    if (!data.isEmpty()) {
        onReadyRead();
    }
}

void MqttClientPrivate::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    emit q_ptr->stateChanged(socketState);
}

quint16 MqttClientPrivate::newPacketId()
{
    static quint16 packetId = 1;
    do {
        packetId++;
    } while (unackedPacketList.contains(packetId));
    return packetId;
}

void MqttClientPrivate::sendPingreq()
{
    MqttPacket packet(MqttPacket::TypePingreq);
    socket->write(packet.serialize());
}

void MqttClientPrivate::restartKeepAliveTimer()
{
    if (keepAlive > 0) {
        keepAliveTimer.start(keepAlive * 1000);
    }
}