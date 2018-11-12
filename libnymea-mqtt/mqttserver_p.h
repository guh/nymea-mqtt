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
    void onNewConnection();
    void onClientReadyRead();
    void onClientError(QAbstractSocket::SocketError);
    void onClientDisconnected();

public:
    MqttServer *q_ptr;

    QTcpServer *server = nullptr;
    MqttUserValidator *userValidator = nullptr;

    Mqtt::QoS maximumSubscriptionQoS = Mqtt::QoS2;

    QHash<QTcpSocket*, QTimer*> pendingConnections;
    QHash<QTcpSocket*, ClientContext*> clientList;
    QHash<QTcpSocket*, QByteArray> clientBuffers;
    QHash<QString, MqttPackets> retainedMessages;
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

#endif // MQTTSERVER_P_H