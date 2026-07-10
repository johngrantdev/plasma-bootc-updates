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
        }

        // TODO: changelog between booted and staged revisions
        QQC2.Label {
            Kirigami.FormData.label: i18n("Changelog:")
            text: i18n("Not implemented yet")
            opacity: 0.6
        }
    }
}
