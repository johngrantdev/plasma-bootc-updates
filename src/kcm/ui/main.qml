import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    Kirigami.FormLayout {
        QQC2.Label {
            Kirigami.FormData.label: i18n("Booted version:")
            text: kcm.bootedVersion.length > 0 ? kcm.bootedVersion : i18n("Unknown")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Image:")
            text: kcm.bootedImage.length > 0 ? kcm.bootedImage : i18n("Unknown")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Update:")
            text: {
                if (!kcm.updateStaged) {
                    return i18n("None staged")
                }
                return kcm.stagedVersion.length > 0
                    ? i18n("Version %1 staged, restart to apply", kcm.stagedVersion)
                    : i18n("Update staged, restart to apply")
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
        }

        RowLayout {
            QQC2.Button {
                text: i18n("Check for Updates")
                icon.name: "view-refresh"
                enabled: !kcm.busy
                onClicked: kcm.checkNow()
            }
            QQC2.Button {
                text: i18n("Download and Stage")
                icon.name: "download"
                enabled: !kcm.busy
                onClicked: kcm.stageNow()
            }
        }

        RowLayout {
            visible: kcm.updateStaged

            QQC2.Button {
                text: i18n("Restart Now")
                icon.name: "system-reboot"
                onClicked: kcm.restartNow()
            }
            QQC2.Button {
                text: i18n("Restart at %1", restartTime.text)
                icon.name: "clock"
                onClicked: {
                    const parts = restartTime.text.split(":")
                    kcm.scheduleRestart(parseInt(parts[0]), parseInt(parts[1]))
                }
            }
            QQC2.TextField {
                id: restartTime
                text: "02:00"
                inputMask: "00:00"
                implicitWidth: Kirigami.Units.gridUnit * 4
            }
        }

        QQC2.Label {
            visible: kcm.lastMessage.length > 0
            text: kcm.lastMessage
            wrapMode: Text.WordWrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 25
        }

        QQC2.BusyIndicator {
            visible: kcm.busy
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            visible: kcm.updateStaged
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Changes:")
            visible: kcm.updateStaged
            text: {
                if (kcm.changelogLoading) {
                    return i18n("Reading package lists…")
                }
                if (kcm.changelogError.length > 0) {
                    return kcm.changelogError
                }
                if (kcm.changelog.length === 0) {
                    return i18n("No package changes")
                }
                return i18np("%1 package changed", "%1 packages changed", kcm.changelogTotal)
            }
            opacity: kcm.changelog.length > 0 ? 1.0 : 0.6
        }

        QQC2.ScrollView {
            visible: kcm.updateStaged && kcm.changelog.length > 0
            implicitWidth: Kirigami.Units.gridUnit * 26
            implicitHeight: Kirigami.Units.gridUnit * 14
            Component.onCompleted: background.visible = true

            ListView {
                clip: true
                model: kcm.changelog // one entry per group
                spacing: 0

                delegate: Column {
                    id: groupDelegate
                    required property var modelData
                    property bool expanded: false
                    width: ListView.view.width

                    QQC2.ItemDelegate {
                        width: parent.width
                        onClicked: groupDelegate.expanded = !groupDelegate.expanded

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: groupDelegate.expanded ? "go-down-symbolic" : "go-next-symbolic"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                            }
                            QQC2.Label {
                                text: groupDelegate.modelData.group
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            QQC2.Label {
                                text: groupDelegate.modelData.count
                                opacity: 0.6
                            }
                        }
                    }

                    // Rows exist only while expanded, so collapsed groups cost
                    // nothing.
                    Repeater {
                        model: groupDelegate.expanded ? groupDelegate.modelData.packages : []

                        delegate: RowLayout {
                            required property var modelData
                            width: groupDelegate.width
                            spacing: Kirigami.Units.largeSpacing

                            Item { implicitWidth: Kirigami.Units.gridUnit }

                            QQC2.Label {
                                text: modelData.name
                                elide: Text.ElideRight
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                            }

                            QQC2.Label {
                                // kind: 0 changed, 1 added, 2 removed
                                text: {
                                    if (modelData.kind === 1) {
                                        return i18n("new, %1", modelData.to)
                                    }
                                    if (modelData.kind === 2) {
                                        return i18n("removed, was %1", modelData.from)
                                    }
                                    return i18n("%1 to %2", modelData.from, modelData.to)
                                }
                                elide: Text.ElideRight
                                opacity: 0.8
                                Layout.fillWidth: true
                            }

                            // Shown when a group mixes sources (manifest mode);
                            // redundant in the vendor fallback.
                            QQC2.Label {
                                visible: modelData.source !== groupDelegate.modelData.group
                                text: modelData.source
                                opacity: 0.6
                                font: Kirigami.Theme.smallFont
                            }
                        }
                    }
                }
            }
        }
    }
}
