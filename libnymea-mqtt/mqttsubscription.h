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


#ifndef MQTTSUBSCRIPTION_H
#define MQTTSUBSCRIPTION_H

#include "mqtt.h"
#include <QString>
#include <QtDebug>

class MqttSubscription
{
public:
    MqttSubscription();
    MqttSubscription(const QByteArray &topicFilter, Mqtt::QoS qoS = Mqtt::QoS0);

    QByteArray topicFilter() const;
    void setTopicFilter(const QByteArray &topicFilter);

    Mqtt::QoS qoS() const;
    void setQoS(Mqtt::QoS qoS);

    bool operator==(const MqttSubscription &other) const;
private:
    QByteArray m_topicFilter;
    Mqtt::QoS m_qoS = Mqtt::QoS0;

};
Q_DECLARE_METATYPE(MqttSubscription)

typedef QVector<MqttSubscription> MqttSubscriptions;
Q_DECLARE_METATYPE(MqttSubscriptions)

inline QDebug operator<<(QDebug debug, const MqttSubscription &subscription) {
    debug.nospace().noquote() << "\"" << subscription.topicFilter() << "\" QoS: " << subscription.qoS();
    return debug;
}

#endif // MQTTSUBSCRIPTION_H
