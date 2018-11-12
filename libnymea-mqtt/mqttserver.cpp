#include "mqttserver.h"
#include "mqttserver_p.h"
#include "mqttpacket.h"

#include <QDebug>
#include <QDataStream>
#include <QUuid>
#include <QtGlobal>
#include <QRegExp>

Q_LOGGING_CATEGORY(dbgServer, "nymea.mqtt.server")

MqttServerPrivate::MqttServerPrivate(MqttServer *q):
    QObject(q),
    q_ptr(q)
{
    qRegisterMetaType<Mqtt::QoS>();

    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &MqttServerPrivate::onNewConnection);
}

QHash<QString, quint16> MqttServerPrivate::publish(const QString &topic, const QByteArray &payload)
{
    QHash<QTcpSocket*, Mqtt::QoS> receivers;
    foreach (QTcpSocket *c, clientList.keys()) {
        foreach (const MqttSubscription &subscription, clientList.value(c)->subscriptions) {
            if (matchTopic(subscription.topicFilter(), topic)) {
                if (!receivers.contains(c) || receivers.value(c) < subscription.qoS()) {
                    receivers[c] = subscription.qoS();
                }
            }
        }
    }

    QHash<QString, quint16> packets;
    foreach (QTcpSocket *receiver, receivers.keys()) {
        ClientContext *ctx = clientList.value(receiver);
        qCDebug(dbgServer) << "Relaying packet to subscribed client:" << ctx->clientId;
        Mqtt::QoS qos = receivers.value(receiver);
        MqttPacket packet(MqttPacket::TypePublish, qos >= Mqtt::QoS0 ? newPacketId(ctx) : 0, qos);
        packet.setTopic(topic.toUtf8());
        packet.setPayload(payload);
        receiver->write(packet.serialize());
        packets.insert(ctx->clientId, packet.packetId());
        if (packet.qos() == Mqtt::QoS0) {
            QString clientId = ctx->clientId;
            QTimer::singleShot(0, this, [this, clientId, packet](){
                emit q_ptr->published(clientId, packet.packetId(), packet.topic(), packet.payload());
            });
        } else {
            ClientContext *ctx = clientList.value(receiver);
            ctx->unackedPackets.insert(packet.packetId(), packet);
            ctx->unackedPacketList.append(packet.packetId());
        }
    }
    return packets;
}

MqttServer::MqttServer(QObject *parent):
    QObject(parent),
    d_ptr(new MqttServerPrivate(this))
{

}

Mqtt::QoS MqttServer::maximumSubscriptionsQoS() const
{
    return d_ptr->maximumSubscriptionQoS;
}

void MqttServer::setMaximumSubscriptionsQoS(Mqtt::QoS maximumSubscriptionQoS)
{
    d_ptr->maximumSubscriptionQoS = maximumSubscriptionQoS;
}

bool MqttServer::listen(const QHostAddress &address, quint16 port, MqttUserValidator *userValidator)
{
    d_ptr->userValidator = userValidator;

    if (!d_ptr->server->listen(address, port)) {
        qCWarning(dbgServer()) << "Error listening on port" << port;
        return false;
    }
    qCDebug(dbgServer) << "nymea MQTT server running on" << address.toString() << ":" << port;
    return true;
}

QStringList MqttServer::clients() const
{
    QStringList clientIds;
    foreach (ClientContext *ctx, d_ptr->clientList) {
        clientIds << ctx->clientId;
    }
    return clientIds;
}

QHash<QString, quint16> MqttServer::publish(const QString &topic, const QByteArray &payload)
{
    return d_ptr->publish(topic, payload);
}

void MqttServerPrivate::onNewConnection()
{
    QTcpSocket *client = server->nextPendingConnection();

    // Start a 10 second timer to clean up the connection if we don't get data until then.
    QTimer *timeoutTimer = new QTimer(this);
    connect(timeoutTimer, &QTimer::timeout, this, [this, client]() {
        qCWarning(dbgServer) << "A client connected but did not send data in 10 seconds. Dropping connection.";
        client->abort();
        pendingConnections.take(client)->deleteLater();
        client->deleteLater();
    });
    timeoutTimer->start(10000);
    pendingConnections.insert(client, timeoutTimer);

    connect(client, &QTcpSocket::readyRead, this, &MqttServerPrivate::onClientReadyRead);
    connect(client, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onClientError(QAbstractSocket::SocketError)));
    connect(client, &QTcpSocket::disconnected, this, &MqttServerPrivate::onClientDisconnected);
}

void MqttServerPrivate::onClientDisconnected()
{
    QTcpSocket *client = static_cast<QTcpSocket*>(sender());
    cleanupClient(client);
}

void MqttServerPrivate::cleanupClient(QTcpSocket *client)
{
    if (clientBuffers.contains(client)) {
        clientBuffers.remove(client);
    }
    if (clientList.contains(client)) {
        ClientContext *ctx = clientList.value(client);
        qCDebug(dbgServer) << "Client" << ctx->clientId << "disconnected.";
        ctx->keepAliveTimer.stop();

        if (!ctx->willTopic.isEmpty()) {
            qCDebug(dbgServer) << "Publishing will message for client" << ctx->clientId << "on topic" << ctx->willTopic << "( Retain:" << ctx->willRetain << ")";
            MqttPacket willPacket(MqttPacket::TypePublish, ctx->willQoS >= Mqtt::QoS1 ? newPacketId(ctx) : 0, ctx->willQoS, ctx->willRetain);
            willPacket.setTopic(ctx->willTopic);
            willPacket.setPayload(ctx->willMessage);
            processPacket(willPacket, client);
        }

        emit q_ptr->clientDisconnected(ctx->clientId);

        clientList.remove(client);
        delete ctx;
    }
    client->flush();
    client->deleteLater();
}

void MqttServerPrivate::processPacket(const MqttPacket &packet, QTcpSocket *client)
{
    if (packet.type() == MqttPacket::TypeConnect) {
        if (clientList.contains(client)) {
            ClientContext *ctx = clientList.value(client);
            qCWarning(dbgServer) << "Client" << ctx->clientId << "sends duplicate CONNECT packets. Dropping connection.";
            cleanupClient(client);
            return;
        }

        MqttPacket response(MqttPacket::TypeConnack, packet.packetId());

        if (packet.protocolLevel() != Mqtt::Protocol311) {
            qCWarning(dbgServer) << "This MQTT broker only supports Protocol version 3.1.1";
            response.setConnectReturnCode(Mqtt::ConnectReturnCodeUnacceptableProtocolVersion);
            client->write(response.serialize());
            cleanupClient(client);
            return;
        }

        QString clientId = packet.clientId();
        if (clientId.isEmpty()) {
            if (!packet.cleanSession()) {
                qCWarning(dbgServer) << "Empty client id provided but clean session flag not set. Rejecting connection.";
                response.setConnectReturnCode(Mqtt::ConnectReturnCodeIdentifierRejected);
                client->write(response.serialize());
                cleanupClient(client);
                return;
            }
            clientId = QUuid::createUuid().toString().remove(QRegExp("[{}-]*"));
        }

        if (userValidator) {
            QString username;
            if (packet.connectFlags().testFlag(Mqtt::ConnectFlagUsername)) {
                username = packet.username();
            }
            QString password;
            if (packet.connectFlags().testFlag(Mqtt::ConnectFlagPassword)) {
                password = packet.password();
            }
            Mqtt::ConnectReturnCode userValidationReturnCode = userValidator->validateConnect(clientId, username, password, client->peerAddress());
            if (userValidationReturnCode != Mqtt::ConnectReturnCodeAccepted) {
                qCWarning(dbgServer) << "Rejecting connection due to user validation.";
                response.setConnectReturnCode(userValidationReturnCode);
                client->write(response.serialize());
                cleanupClient(client);
                return;
            }
        }

        ClientContext *ctx = nullptr;

        QList<QTcpSocket*> existingSockets = clientList.keys();
        for (int i = 0; i < existingSockets.count(); i++) {
            QTcpSocket *existingClient = existingSockets.at(i);
            if (clientId == clientList.value(existingClient)->clientId) {
                if (!packet.connectFlags().testFlag(Mqtt::ConnectFlagCleanSession)) {
                    qCDebug(dbgServer).nospace() << clientId << ": Already have a session for this client ID. Taking over existing session.";

                    response.setConnackFlags(Mqtt::ConnackFlagSessionPresent);
                    ctx = clientList.value(existingClient);

                    // remove old client manually, we don't want to clean up the context, nor send any will message or emit disconnected signals
                    clientList.remove(existingClient);
                    clientBuffers.remove(existingClient);
                    existingClient->flush();
                    existingClient->deleteLater();
                } else {
                    qCDebug(dbgServer).nospace() << clientId << ": Already have a session for this client ID. Dropping old session.";
                    cleanupClient(existingClient);
                }
                break;
            }
        }

        if (!ctx) {
            if (!packet.connectFlags().testFlag(Mqtt::ConnectFlagCleanSession)) {
                qCWarning(dbgServer).nospace() << clientId << ": Request to take over existing session but we don't have an existing session.";
            }

            ctx = new ClientContext();
            ctx->clientId = clientId;

            connect(&ctx->keepAliveTimer, &QTimer::timeout, this, [this, client](){
                qCWarning(dbgServer) << "Keep alive timeout reached for client:" << clientList.value(client)->clientId;
                cleanupClient(client);
            });
        }

        ctx->keepAlive = packet.keepAlive();
        ctx->version = packet.protocolLevel();


        if (packet.connectFlags().testFlag(Mqtt::ConnectFlagWill)) {
            ctx->willTopic = packet.willTopic();
            ctx->willMessage = packet.willMessage();
            ctx->willRetain = packet.willRetain();
            if (packet.connectFlags().testFlag(Mqtt::ConnectFlagWillQoS2)) {
                ctx->willQoS = Mqtt::QoS2;
            } else if (packet.connectFlags().testFlag(Mqtt::ConnectFlagWillQoS1)) {
                ctx->willQoS = Mqtt::QoS1;
            }
        }
        if (packet.connectFlags().testFlag(Mqtt::ConnectFlagUsername)) {
            ctx->username = packet.username();
        }
        if (packet.connectFlags().testFlag(Mqtt::ConnectFlagPassword)) {
        }

        qCDebug(dbgServer).nospace().noquote()
                << "New MQTT client: \"" << clientId << '\"'
                << ", Protocol: " << packet.protocolName() << " (" << packet.protocolLevel() << ')'
                << ", Flags: " << packet.connectFlags()
                << ", KeepAlive: " << packet.keepAlive()
                << ", Will Topic: \"" << packet.willTopic() << '\"'
                << ", Will Message: \"" << packet.willMessage() << '\"'
                << ", Will Retain: " << packet.willRetain()
                << ", Username: " << packet.username()
                << ", Password: " << QString(packet.password()).replace(QRegExp("."), "*");

        if (ctx->keepAlive > 0) {
            ctx->keepAliveTimer.start(ctx->keepAlive * 1500);
        }

        clientList.insert(client, ctx);
        response.setConnectReturnCode(Mqtt::ConnectReturnCodeAccepted);
        client->write(response.serialize());
        emit q_ptr->clientConnected(ctx->clientId, ctx->username, client->peerAddress());

        foreach (quint16 retryPacketId, ctx->unackedPacketList) {
            qCDebug(dbgServer) << "Resending unacked packet" << retryPacketId << "to" << ctx->clientId;;
            MqttPacket retryPacket = ctx->unackedPackets.value(retryPacketId);
            retryPacket.setDup(true);
            client->write(retryPacket.serialize());
        }
        return;
    }

    if (!clientList.contains(client)) {
        qCWarning(dbgServer) << "Protocol error: Client connection did not send CONNECT yet. Dropping connection.";
        client->close();
        return;
    }

    ClientContext *ctx = clientList.value(client);
    if (ctx->keepAlive > 0) {
        ctx->keepAliveTimer.start();
    }
    emit q_ptr->clientAlive(ctx->clientId);

    if (packet.type() == MqttPacket::TypePublish) {
        qCDebug(dbgServer).nospace() << "Publish received from client " << ctx->clientId << ": Topic: " << packet.topic() << ", Payload: " << packet.payload() << " (Packet ID: " << packet.packetId() << ", DUP: " << packet.dup() << ", QoS: " << packet.qos() << ", Retain: " << packet.retain() << ')';
        emit q_ptr->publishReceived(ctx->clientId, packet.packetId(), packet.topic(), packet.payload(), packet.dup());

        switch (packet.qos()) {
        case Mqtt::QoS0:
            break;
        case Mqtt::QoS1: {
            MqttPacket response(MqttPacket::TypePuback, packet.packetId());
            client->write(response.serialize());
            break;
        }
        case Mqtt::QoS2: {
            if (packet.dup() && ctx->unackedPacketList.contains(packet.packetId())) {
                // We received this message before but the client keeps on trying... Just send a PUBREC and stop processing
                client->write(ctx->unackedPackets.value(packet.packetId()).serialize());
                return;
            } else if (ctx->unackedPacketList.contains(packet.packetId())) {
                // Hmm... Client says this is a new packet, but the ID is not released yet! Drop client connection.
                qCWarning(dbgServer()).nospace() << "Received a bad packet from \"" << ctx->clientId << "\". DUP is not set but packet ID is already used and not released. Dropping client connection.";
                cleanupClient(client);
                return;
            }
            // Ok, a new packet, ack it with a PUBREC and store the number
            MqttPacket response(MqttPacket::TypePubrec, packet.packetId());
            ctx->unackedPackets.insert(response.packetId(), response);
            ctx->unackedPacketList.append(packet.packetId());
            client->write(response.serialize());
            break;
        }
        }
        if (packet.retain()) {
            if (packet.payload().isEmpty()) {
                qCDebug(dbgServer) << "Clearing retained messages for topic" << packet.topic();
                retainedMessages.remove(packet.topic());
            } else {
                if (packet.qos() == Mqtt::QoS0) {
                    qCDebug(dbgServer) << "Clearing retained messages for topic" << packet.topic();
                    retainedMessages.remove(packet.topic());
                }
                qCDebug(dbgServer) << "Adding retained message for topic" << packet.topic();
                retainedMessages[packet.topic()].append(packet);
            }
        }

        publish(packet.topic(), packet.payload());

        return;
    }
    if (packet.type() == MqttPacket::TypePuback) {
        ctx->unackedPacketList.removeAll(packet.packetId());
        MqttPacket publishedPacket = ctx->unackedPackets.take(packet.packetId());
        emit q_ptr->published(ctx->clientId, packet.packetId(), publishedPacket.topic(), publishedPacket.payload());
        return;
    }
    if (packet.type() == MqttPacket::TypePubrec) {
        MqttPacket publishedPacket = ctx->unackedPackets.take(packet.packetId());
        emit q_ptr->published(ctx->clientId, packet.packetId(), publishedPacket.topic(), publishedPacket.payload());
        MqttPacket pubrel(MqttPacket::TypePubrel, packet.packetId());
        ctx->unackedPackets.insert(packet.packetId(), pubrel);
        client->write(pubrel.serialize());
        return;
    }
    if (packet.type() == MqttPacket::TypePubrel) {
        ctx->unackedPackets.remove(packet.packetId());
        ctx->unackedPacketList.removeAll(packet.packetId());
        MqttPacket response(MqttPacket::TypePubcomp, packet.packetId());
        client->write(response.serialize());
        return;
    }
    if (packet.type() == MqttPacket::TypePubcomp) {
        ctx->unackedPackets.remove(packet.packetId());
        ctx->unackedPacketList.removeAll(packet.packetId());
        return;
    }
    if (packet.type() == MqttPacket::TypeSubscribe) {
//        qCDebug(dbgServer).nospace() << ctx->clientId ": Subscribe packet received.";
        MqttPacket response(MqttPacket::TypeSuback, packet.packetId());
        QByteArray payload;
        foreach (MqttSubscription subscription, packet.subscriptions()) {
            if (userValidator && !userValidator->validateSubscribe(subscription.topicFilter(), ctx->clientId, ctx->username)) {
                qCWarning(dbgServer).nospace().noquote() << "Subscription topic filter not allowed for client \"" << ctx->clientId << "\": \"" << subscription.topicFilter() << '\"';
                response.addSubscribeReturnCode(Mqtt::SubscribeReturnCodeFailure);
                continue;
            }
            if (!validateTopicFilter(subscription.topicFilter())) {
                qCWarning(dbgServer).nospace() << "Subscription topic filter not valid for client \"" << ctx->clientId << "\": " << subscription.topicFilter();
                response.addSubscribeReturnCode(Mqtt::SubscribeReturnCodeFailure);
                continue;
            }
            subscription.setQoS(qMin(subscription.qoS(), maximumSubscriptionQoS));
            bool updated = false;
            for (int i = 0; i < ctx->subscriptions.count(); i++) {
                if (ctx->subscriptions.at(i).topicFilter() == subscription.topicFilter()) {
                    qCDebug(dbgServer).noquote().nospace() << "Client \"" << ctx->clientId << "\" subscribed with a duplicate topic filter. Replacing subcription with new QoS" << subscription.qoS();
                    ctx->subscriptions.replace(i, subscription);
                    updated = true;
                }
            }
            if (!updated) {
                ctx->subscriptions.append(subscription);
            }
            qCDebug(dbgServer).noquote().nospace() << "Subscribed client \"" << ctx->clientId << "\" to topic filter: \"" << subscription.topicFilter() << "\" with QoS " << subscription.qoS();
            emit q_ptr->clientSubscribed(ctx->clientId, subscription.topicFilter(), subscription.qoS());
            switch (subscription.qoS()) {
            case Mqtt::QoS0:
                response.addSubscribeReturnCode(Mqtt::SubscribeReturnCodeSuccessQoS0);
                break;
            case Mqtt::QoS1:
                response.addSubscribeReturnCode(Mqtt::SubscribeReturnCodeSuccessQoS1);
                break;
            case Mqtt::QoS2:
                response.addSubscribeReturnCode(Mqtt::SubscribeReturnCodeSuccessQoS2);
                break;
            }
        }
        client->write(response.serialize());

        // Deliver any retained messages for this topic
        foreach (MqttSubscription subscription, packet.subscriptions()) {
            foreach (const QString &topic, retainedMessages.keys()) {
                if (matchTopic(subscription.topicFilter(), topic)) {
                    foreach (MqttPacket packet, retainedMessages.value(topic)) {
                        packet.setRetain(true);
                        client->write(packet.serialize());
                    }
                }
            }
        }
        return;
    }
    if (packet.type() == MqttPacket::TypeUnsubscribe) {
        MqttSubscriptions newSubscriptions;
        foreach (const MqttSubscription &existingSubscription, ctx->subscriptions) {
            bool matching = false;
            foreach (const MqttSubscription &unsub, packet.subscriptions()) {
                if (existingSubscription.topicFilter() == unsub.topicFilter()) {
                    qCDebug(dbgServer) << "Unsubscribing client" << ctx->clientId << "from" << unsub.topicFilter();
                    emit q_ptr->clientUnsubscribed(ctx->clientId, unsub.topicFilter());
                    matching = true;
                    break;
                }
            }
            if (!matching) {
                newSubscriptions.append(existingSubscription);
            }
        }
        ctx->subscriptions = newSubscriptions;
        MqttPacket response(MqttPacket::TypeUnsuback, packet.packetId());
        client->write(response.serialize());
        return;
    }
    if (packet.type() == MqttPacket::TypePingreq) {
//        qCDebug(dbgServer).nospace() << ctx->clientId << ": Pingreq received";
        MqttPacket response(MqttPacket::TypePingresp, packet.packetId());
        client->write(response.serialize());
        return;
    }
    if (packet.type() == MqttPacket::TypeDisconnect) {
        ctx->willMessage.clear();
        ctx->willTopic.clear();
        return;
    }
    qCWarning(dbgServer).nospace().noquote() << "Unknown packet received from client \"" << ctx->clientId << "\": " << QString::number(packet.type(), 16);
    Q_ASSERT(false);
    cleanupClient(client);

}

bool MqttServerPrivate::validateTopicFilter(const QString &topicFilter)
{
    if (topicFilter.length() < 1) {
        return false;
    }
    QStringList parts = topicFilter.split('/');
    for (int i = 0; i < parts.count(); i++) {
        const QString &part = parts.at(i);
        if (part.contains('#') && (part != '#' || i != parts.count() - 1)) {
            return false;
        }
        if (part.contains('+') && part != '+') {
            return false;
        }
    }

    return true;
}

bool MqttServerPrivate::matchTopic(const QString &topicFilter, const QString &topic)
{
    if (topic.startsWith('$')) {
        return false;
    }

    QStringList filterParts = topicFilter.split('/');
    QStringList topicParts = topic.split('/');

    if (topicParts.count() < filterParts.count() - 1) {
        return false;
    }

    for (int i = 0; i < filterParts.count(); i++) {
        if (filterParts.at(i) == '+') {
            continue;
        }
        if (filterParts.at(i) == '#') {
            continue;
        }
        if (topicParts.at(i) == filterParts.at(i)) {
            continue;
        }
        return false;
    }

    return filterParts.count() == topicParts.count() || topicFilter.endsWith('#');
}

quint16 MqttServerPrivate::newPacketId(ClientContext *ctx)
{
    static quint16 packetId = 0;
    do {
        packetId++;
    } while(ctx->unackedPacketList.contains(packetId));
    return packetId;
}

void MqttServerPrivate::onClientReadyRead()
{
    QTcpSocket* client = static_cast<QTcpSocket*>(sender());

    clientBuffers[client].append(client->readAll());

    do {
        MqttPacket packet;
        int ret = packet.parse(clientBuffers[client]);
        if (ret == 0) {
            qCDebug(dbgServer) << "Packet too short... Waiting for more...";
            return;
        }

        // Ok, we've got a full packet (or garbage data). If this client is still pending
        // we can stop the timer, the protocol will take it from here.
        if (pendingConnections.contains(client)) {
            pendingConnections.take(client)->deleteLater();
        }

        if (ret == -1) {
            qCWarning(dbgServer) << "Bad MQTT packet data, Dropping connection";
            cleanupClient(client);
            return;
        }

        clientBuffers[client].remove(0, ret);

        processPacket(packet, client);

    } while (!clientBuffers.value(client).isEmpty());
}

void MqttServerPrivate::onClientError(QAbstractSocket::SocketError error)
{
//    qCWarning(dbgServer) << "Client error:" << error;
}