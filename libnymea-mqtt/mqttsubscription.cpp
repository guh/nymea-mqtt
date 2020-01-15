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

#include "mqttsubscription.h"

MqttSubscription::MqttSubscription()
{

}

MqttSubscription::MqttSubscription(const QByteArray &topicFilter, Mqtt::QoS qoS):
    m_topicFilter(topicFilter),
    m_qoS(qoS)
{

}

QByteArray MqttSubscription::topicFilter() const
{
    return m_topicFilter;
}

void MqttSubscription::setTopicFilter(const QByteArray &topicFilter)
{
    m_topicFilter = topicFilter;
}

Mqtt::QoS MqttSubscription::qoS() const
{
    return m_qoS;
}

void MqttSubscription::setQoS(Mqtt::QoS qoS)
{
    m_qoS = qoS;
}

bool MqttSubscription::operator==(const MqttSubscription &other) const
{
    return m_qoS == other.qoS() && m_topicFilter == other.topicFilter();
}
