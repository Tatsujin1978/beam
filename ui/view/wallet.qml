import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.0
import "controls"

Item {
    id: root
    anchors.fill: parent

    state: "wallet"

    Rectangle {
        id: send_layout
        anchors.fill: parent
        anchors.topMargin: 97
        anchors.bottomMargin: 30

        radius: 10
        color: Style.dark_slate_blue

        Item {
            anchors.fill: parent
            anchors.topMargin: 30
            anchors.bottomMargin: 30
            anchors.leftMargin: 30
            anchors.rightMargin: 30

            clip: true

            Item {
                anchors.fill: parent
                anchors.rightMargin: parent.width/2
                anchors.bottomMargin: 60

                clip: true

                SFText {
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: Style.white
                    text: "Send BEAM"
                }

                SFText {
                    id: rec_addr

                    y: 41

                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: Style.white
                    text: "Recipient address"
                }

                TextInput {
                    id: receiver_addr

                    y: 115-30
                    width: 300

                    font.pixelSize: 12
                    font.family: rec_addr.family

                    color: Style.white

                    text: walletViewModel.receiverAddr

                }

                Binding {
                    target: walletViewModel
                    property: "receiverAddr"
                    value: receiver_addr.text
                }

                Rectangle {
                    y: 109
                    width: 300
                    height: 1

                    color: "#33566b"
                }
            }

            Item {
                anchors.fill: parent
                anchors.leftMargin: parent.width/2
                anchors.bottomMargin: 60

                SFText {
                    id: tx_amout_label
                    y: 41

                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: Style.white
                    text: "Transaction amount"
                }

                TextInput {
                    id: amount_input
                    y: 93-30
                    width: 300

                    font.pixelSize: 48
                    font.family: tx_amout_label.family

                    color: Style.heliotrope

                    text: walletViewModel.sendAmount
                }

                Binding {
                    target: walletViewModel
                    property: "sendAmount"
                    value: amount_input.text
                }

                Rectangle {
                    y: 153-30
                    width: 337
                    height: 1

                    color: "#33566b"
                }

                SFText {
                    x: 204+157
                    y: 117-30

                    font.pixelSize: 24
                    color: Style.white
                    text: "BEAM"
                }

                SFText {
                    y: 164-30
                    opacity: 0.5
                    font.pixelSize: 24
                    font.weight: Font.ExtraLight
                    color: Style.white
                    text: amount_input.text + " USD"
                }

                /////////////////////////////////////////////////////////////
                /// Transaction fee /////////////////////////////////////////
                /////////////////////////////////////////////////////////////

                SFText {
                    y: 243-30   

                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: Style.white
                    text: "Transaction fee"
                }

                Rectangle {
                    id: fee_line
                    y: 303-30
                    width: 360
                    height: 4

                    opacity: 0.1
                    radius: 2

                    color: Style.white
                }

                Rectangle {
                    id: led

                    x: 140
                    y: 303-30-8

                    width: 20
                    height: 20

                    radius: 10

                    color: Style.bright_teal

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                DropShadow {
                    anchors.fill: led
                    radius: 5
                    samples: 9
                    color: Style.bright_teal
                    source: led
                }

                SFText {
                    y: 277-30

                    font.pixelSize: 12
                    color: Style.bluey_grey
                    text: "40h"
                }

                SFText {
                    y: 277-30
                    anchors.right: fee_line.right

                    font.pixelSize: 12
                    color: Style.bluey_grey
                    text: "20m"
                }

                SFText {
                    y: 319-30

                    font.pixelSize: 12
                    color: Style.bluey_grey
                    text: "0.0002"
                }

                SFText {
                    y: 319-30
                    anchors.right: fee_line.right

                    font.pixelSize: 12
                    color: Style.bluey_grey
                    text: "0.01"
                }

                /////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////
                /////////////////////////////////////////////////////////////

                SFText {
                    y: 383-30   

                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: Style.white
                    text: "Comment"
                }

                TextInput {
                    y: 427-30
                    width: 300

                    font.pixelSize: 12
                    font.family: tx_amout_label.family

                    color: Style.white

                    text: "Thank you for your work!"
                }

                Rectangle {
                    y: 451-30
                    width: 339
                    height: 1

                    color: "#33566b"
                }
            }

            Row {

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom

                spacing: 30

                Rectangle {
                    width: 130
                    height: 40

                    radius: 20

                    color: "#33566b"

                    SFText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: Style.white
                        text: "CANCEL"

                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.state = "wallet"
                    }
                }

                Rectangle {
                    width: 108
                    height: 40

                    radius: 20

                    color: Style.heliotrope

                    SFText {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: Style.white
                        text: "SEND"
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            walletViewModel.sendMoney()
                            root.state = "wallet"
                        }
                    }
                }                
            }

        }

        visible: false
    }

    Rectangle {

        id: wallet_layout
        anchors.fill: parent
        color: Style.marine
        state: "wide"

        CuteButton {
            anchors.top: parent.top
            anchors.right: parent.right

            label: "SEND"

            onClicked: root.state = "send"
        }

        Item {
            y: 97
            height: 206

            anchors.left: parent.left
            anchors.right: parent.right

            Row {

                id: wide_panels

                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.height

                spacing: 30

                AvailablePanel {
                    width: (parent.width - 3*30)*500/1220
                    height: parent.height
                }

                SecondaryPanel {
                    width: (parent.width - 3*30)*240/1220
                    height: parent.height
                    title: "Received"
                    amountColor: Style.bright_sky_blue
                    value: walletViewModel.received
                }

                SecondaryPanel {
                    width: (parent.width - 3*30)*240/1220
                    height: parent.height
                    title: "Sent"
                    amountColor: Style.heliotrope
                    value: walletViewModel.sent
                }

                SecondaryPanel {
                    width: (parent.width - 3*30)*240/1220
                    height: parent.height
                    title: "Unconfirmed"
                    amountColor: Style.white
                    value: walletViewModel.unconfirmed
                }
            }

            Row {

                id: medium_panels

                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.height

                spacing: 30

                AvailablePanel {
                    width: (parent.width - parent.spacing)*518/864
                    height: parent.height              
                }

                Item {
                    id: panel_holder

                    width: (parent.width - parent.spacing)*346/864
                    height: parent.height
                    state: "one"

                    clip: true
                    
                    SecondaryPanel {
                        id: received_panel
                        title: "Received"
                        amountColor: Style.bright_sky_blue
                        value: walletViewModel.received
                        anchors.fill: parent
                        visible: true
                    }

                    SecondaryPanel {
                        id: sent_panel
                        title: "Sent"
                        amountColor: Style.heliotrope
                        value: walletViewModel.sent
                        anchors.fill: parent
                        visible: false
                    }

                    SecondaryPanel {
                        id: unconfirmed_panel
                        title: "Unconfirmed"
                        amountColor: Style.white
                        value: walletViewModel.unconfirmed
                        anchors.fill: parent
                        visible: false
                    }

                    Row {

                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10

                        spacing: 10

                        Led {
                            id: led1

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel_holder.state = "one"
                            }
                        }
                        
                        Led {
                            id: led2

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel_holder.state = "two"
                            }
                        }

                        Led {
                            id: led3

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel_holder.state = "three"
                            }
                        }
                    }

                    states: [
                        State {
                            name: "one"
                            PropertyChanges {target: led1; turned_on: true}
                        },
                        State {
                            name: "two"
                            PropertyChanges {target: sent_panel; visible: true}
                            PropertyChanges {target: led2; turned_on: true}
                        },
                        State {
                            name: "three"
                            PropertyChanges {target: unconfirmed_panel; visible: true}
                            PropertyChanges {target: led3; turned_on: true}
                        }
                    ]
                }

                
                visible: false
            }
        }

        Item
        {
            y: 353

            anchors.left: parent.left
            anchors.right: parent.right

            SFText {
                x: 30

                font {
                    pixelSize: 18
                    weight: Font.Bold
                }

                color: Style.white

                text: "Transactions"
            }

            Row {

                anchors.right: parent.right
                spacing: 20
                state: "all"

                TxFilter{
                    id: all
                    label: "ALL"
                    onClicked: parent.state = "all"
                }

                TxFilter{
                    id: sent
                    label: "SENT"
                    onClicked: parent.state = "sent"
                }

                TxFilter{
                    id: received
                    label: "RECEIVED"
                    onClicked: parent.state = "received"
                }

                TxFilter{
                    id: in_progress
                    label: "IN PROGRESS"
                    onClicked: parent.state = "in_progress"
                }

                states: [
                    State {
                        name: "all"
                        PropertyChanges {target: all; state: "active"}
                    },
                    State {
                        name: "sent"
                        PropertyChanges {target: sent; state: "active"}
                    },
                    State {
                        name: "received"
                        PropertyChanges {target: received; state: "active"}
                    },
                    State {
                        name: "in_progress"
                        PropertyChanges {target: in_progress; state: "active"}
                    }
                ]
            }        
        }

        Rectangle {
            anchors.fill: parent;
            anchors.topMargin: 394

            radius: 10

            color: Style.dark_slate_blue
        }

        Rectangle {
            anchors.fill: parent;
            anchors.topMargin: 394+46

            color: "#0a344d"
        }

        TableView {

            anchors.fill: parent;
            anchors.topMargin: 394

            frameVisible: false
            selectionMode: SelectionMode.NoSelection
            backgroundVisible: false

            TableViewColumn {
                role: "income"
                width: 72

                resizable: false
                movable: false

                delegate: Item {

                    anchors.fill: parent

                    SvgImage {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                        source: styleData.value ? "qrc:///assets/icon-received.svg" : "qrc:///assets/icon-sent.svg"
                    }
                }
            }

            TableViewColumn {
                role: "date"
                title: "Date | time"
                width: (300-72)

                resizable: false
                movable: false
            }

            TableViewColumn {
                role: "recipient"
                title: "Recipient / Sender ID"
                width: (680-300)

                resizable: false
                movable: false
            }

            TableViewColumn {
                role: "comment"
                title: "Comment"
                width: (800-680)

                resizable: false
                movable: false

                delegate: Item {

                    anchors.fill: parent

                    SvgImage {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 20
                        source: "qrc:///assets/icon-comment.svg"
                        visible: styleData.value != null
                    }
                }
            }

            TableViewColumn {
                role: "amount"
                title: "Amount, BEAM"
                width: (1000-800)

                resizable: false
                movable: false

                delegate: Row {
                    anchors.fill: parent
                    spacing: 6

                    property bool income: model["income"]

                    SFText {
                        font.pixelSize: 24

                        color: parent.income ? Style.bright_sky_blue : Style.heliotrope

                        anchors.verticalCenter: parent.verticalCenter
                        text: (parent.income ? "+ " : "- ") + styleData.value
                    }

                    SFText {
                        font.pixelSize: 12

                        color: parent.income ? Style.bright_sky_blue : Style.heliotrope

                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 4
                        text: "BEAM"
                    }
                }
            }

            TableViewColumn {
                role: "amount_usd"
                title: "Amount, USD"
                width: (1214-1000)

                resizable: false
                movable: false
            }

            TableViewColumn {
                role: "status"
                title: "Status"
                width: (34+62)

                resizable: false
                movable: false

                delegate: Item {

                    anchors.fill: parent

                    SFText {
                        font.pixelSize: 12

                        color: {
                            if(styleData.value == "sent")
                                Style.heliotrope
                            else if(styleData.value == "received")
                                Style.bright_sky_blue
                            else Style.white
                        }

                        anchors.verticalCenter: parent.verticalCenter
                        text: styleData.value
                    }
                }
            }

            model: walletViewModel.tx

            // model: ListModel {
            //     ListElement {
            //         income: true
            //         date: "12 June 2018  |  3:46 PM"
            //         recipient: "1Cs4wu6pu5qCZ35bSLNVzGyEx5N6uzbg9t"
            //         comment: "Thanks for your work!"
            //         amount: "0.63736"
            //         amount_usd: "726.4 USD"
            //         status: "received"
            //     }

            //     ListElement {
            //         income: false
            //         date: "10 June 2018  |  7:02 AM"
            //         recipient: "magic_stardust16"
            //         amount: "1.300"
            //         amount_usd: "10 726.4 USD"
            //         status: "sent"
            //     }
            // }

            headerDelegate: Item {
                height: 46

                SFText {
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: 12
                    color: Style.bluey_grey

                    text: styleData.value
                }
            }

            rowDelegate: Item {
                height: 69

                anchors.left: parent.left
                anchors.right: parent.right

                Rectangle {
                    anchors.fill: parent

                    color: Style.light_navy
                    visible: styleData.alternate
                }
            }

            itemDelegate: Item {

                anchors.fill: parent

                SFText {
                    anchors.verticalCenter: parent.verticalCenter

                    font.pixelSize: 12
                    color: Style.white

                    font.weight: Font.Normal

                    text: styleData.value
                }
            }
        }
        
        states: [
            State {
                name: "wide"
            },

            State {
                when: wallet_layout.visible && wallet_layout.width < (1440-70-2*30)
                name: "medium"
                PropertyChanges {target: wide_panels; visible: false}
                PropertyChanges {target: medium_panels; visible: true}
            },

            State {
                when: wallet_layout.visible && wallet_layout.width < (1440-70-2*30)
                name: "small"
                PropertyChanges {target: wide_panels; visible: false}
                PropertyChanges {target: medium_panels; visible: true}
            }
        ]
    }    

    states: [
        State {
            name: "wallet"
        },

        State {
            name: "send"
            PropertyChanges {target: wallet_layout; visible: false}
            PropertyChanges {target: send_layout; visible: true}
        }
    ]
}
