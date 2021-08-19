import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Dialog {
    id: dialog
    title: qsTr("A propos")
    modal: true
    x: Math.round((window.width / 2) - (width / 2) - 220)
    y: Math.round((window.height / 2) - (height / 2))
    standardButtons: DialogButtonBox.Ok

    onAccepted: dialog.close()

    contentItem: ColumnLayout {
        spacing: 10

        Text {
            id: txt1
            wrapMode: Text.WordWrap
            text: qsTr("Swift Assistant est un projet open source qui a \npour but de vous aider dans vos taches quotidiennes, il fonctionne \nsur la base de plugins. Si vous voulez contribuer au projet voici les liens:")
        }

        Text {
            id: txt2
            wrapMode: Text.WordWrap
            text: qsTr("Le dépot du projet: https://github.com/Swiftapp-hub/Swift-Assistant")
        }

        Text {
            id: txt3
            wrapMode: Text.WordWrap
            text: qsTr("La documentation pour créer un plugin: ")
        }

        Text {
            id: txt4
            wrapMode: Text.WordWrap
            font.bold: true
            text: qsTr("Version: 1.0.1")
        }
    }
}
