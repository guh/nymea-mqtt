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

#ifndef MQTT_H
#define MQTT_H

#include <QObject>

namespace Mqtt {

enum Protocol {
    ProtocolUnknown = 0x00,
    Protocol311 = 0x04
};

enum QoS {
    QoS0 = 0x00,
    QoS1 = 0x01,
    QoS2 = 0x02
};

enum ConnectFlag {
    ConnectFlagNone = 0x00,
    ConnectFlagCleanSession = 0x02,
    ConnectFlagWill = 0x04,
    ConnectFlagWillQoS1 = 0x08,
    ConnectFlagWillQoS2 = 0x10,
    ConnectFlagWillRetain = 0x20,
    ConnectFlagPassword = 0x40,
    ConnectFlagUsername = 0x80
};
Q_DECLARE_FLAGS(ConnectFlags, ConnectFlag)

enum ConnackFlag {
    ConnackFlagNone = 0x0,
    ConnackFlagSessionPresent = 0x1
};
Q_DECLARE_FLAGS(ConnackFlags, ConnackFlag)

enum ConnectReturnCode {
    ConnectReturnCodeAccepted = 0x00,
    ConnectReturnCodeUnacceptableProtocolVersion = 0x01,
    ConnectReturnCodeIdentifierRejected = 0x02,
    ConnectReturnCodeServerUnavailable = 0x03,
    ConnectReturnCodeBadUsernameOrPassword = 0x04,
    ConnectReturnCodeNotAuthorized = 0x05
};
enum SubscribeReturnCode {
    SubscribeReturnCodeSuccessQoS0 = 0x00,
    SubscribeReturnCodeSuccessQoS1 = 0x01,
    SubscribeReturnCodeSuccessQoS2 = 0x02,
    SubscribeReturnCodeFailure = 0x80
};
typedef QList<SubscribeReturnCode> SubscribeReturnCodes;

};

Q_DECLARE_METATYPE(Mqtt::QoS)
Q_DECLARE_METATYPE(Mqtt::ConnectFlags)
Q_DECLARE_METATYPE(Mqtt::ConnackFlags)
Q_DECLARE_METATYPE(Mqtt::ConnectReturnCode)
Q_DECLARE_METATYPE(Mqtt::SubscribeReturnCode)
Q_DECLARE_METATYPE(Mqtt::SubscribeReturnCodes)

#endif // MQTT_H
