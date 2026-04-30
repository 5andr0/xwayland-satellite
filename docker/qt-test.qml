import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12

ApplicationWindow {
    id: appWindow
    property int customTitleBarHeight: testUseTitleBar ? 0 : 46
    property int titleBarButtonWidth: 52
    property bool titleBarMinMaxEnabled: testTitleBarMinMax
    property int titleBarControlCount: titleBarMinMaxEnabled ? 3 : 1
    property bool maximized: visibility === Window.Maximized
    visible: true
    title: testUseTitleBar ? "window-test" : ""
    minimumWidth: testMinWidth
    minimumHeight: testMinHeight
    width: testInitialWidth
    height: testInitialHeight
    flags: testUseTitleBar
        ? (Qt.Window | Qt.WindowTitleHint | Qt.WindowSystemMenuHint
            | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint)
        : (Qt.FramelessWindowHint | Qt.CustomizeWindowHint | Qt.WindowSystemMenuHint
            | Qt.WindowMinimizeButtonHint | Qt.WindowMaximizeButtonHint | Qt.WindowCloseButtonHint | Qt.Window)

    Rectangle {
        id: customTitleBar
        visible: !testUseTitleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: appWindow.customTitleBarHeight
        color: "#202428"
        border.color: "#31363b"
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#3e454d"
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 16
            color: "#f2f4f7"
            text: "window-test"
            font.pixelSize: 14
        }

        Rectangle {
            id: closeButton
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: appWindow.titleBarButtonWidth
            color: closeArea.containsMouse ? "#c94f4f" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "x"
                color: "#f2f4f7"
                font.pixelSize: 16
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Qt.quit()
            }
        }

        Rectangle {
            id: maximizeButton
            anchors.right: closeButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: appWindow.titleBarMinMaxEnabled
            width: appWindow.titleBarMinMaxEnabled ? appWindow.titleBarButtonWidth : 0
            color: maximizeArea.containsMouse ? "#2f363d" : "transparent"

            Text {
                anchors.centerIn: parent
                text: appWindow.maximized ? "R" : "M"
                color: "#f2f4f7"
                font.pixelSize: 16
            }

            MouseArea {
                id: maximizeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (appWindow.maximized) {
                        appWindow.showNormal()
                    } else {
                        appWindow.showMaximized()
                    }
                }
            }
        }

        Rectangle {
            id: minimizeButton
            anchors.right: maximizeButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: appWindow.titleBarMinMaxEnabled
            width: appWindow.titleBarMinMaxEnabled ? appWindow.titleBarButtonWidth : 0
            color: minimizeArea.containsMouse ? "#2f363d" : "transparent"

            Text {
                anchors.centerIn: parent
                text: "_"
                color: "#f2f4f7"
                font.pixelSize: 18
            }

            MouseArea {
                id: minimizeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: appWindow.showMinimized()
            }
        }

        MouseArea {
            enabled: !testUseTitleBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.rightMargin: appWindow.titleBarButtonWidth * appWindow.titleBarControlCount
            property var previousPosition
            propagateComposedEvents: true
            onPressed: previousPosition = globalCursor.getPosition()
            onPositionChanged: {
                if (pressedButtons !== Qt.LeftButton) {
                    return
                }

                var pos = globalCursor.getPosition()
                var dx = pos.x - previousPosition.x
                var dy = pos.y - previousPosition.y

                appWindow.x += dx
                appWindow.y += dy
                previousPosition = pos
            }
        }
    }

    Loader {
        id: bodyLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: customTitleBar.visible ? customTitleBar.bottom : parent.top
        anchors.bottom: parent.bottom
        active: true
        sourceComponent: Rectangle {
            color: "#111418"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 20
                radius: 12
                color: "#1a1f24"
                border.color: "#2d3339"

                Column {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 18
                    spacing: 8

                    Text {
                        text: testUseTitleBar ? "System title bar enabled" : "Custom draggable title bar enabled"
                        color: "#f2f4f7"
                        font.pixelSize: 20
                    }

                    Text {
                        text: testUseTitleBar
                            ? "Set TEST_SSD=0 to test frameless custom decorations."
                            : (appWindow.titleBarMinMaxEnabled
                                ? "Drag the top bar to move the window. The custom bar exposes minimize, maximize, and close controls."
                                : "Drag the top bar to move the window. Set TEST_TITLEBAR_MINMAX=1 to add minimize and maximize controls.")
                        color: "#b4bcc4"
                        wrapMode: Text.WordWrap
                        width: 420
                        font.pixelSize: 13
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (screenAvailableWidth > width) {
            x = (screenAvailableWidth - width) / 2
        }
        if (screenAvailableHeight > height) {
            y = (screenAvailableHeight - height) / 2
        }
        console.log("qt test requested position", x, y, "size", width, height)
    }
}