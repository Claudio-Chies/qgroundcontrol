import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

SetupPage {
    pageComponent:  pageComponent
    Component {
        id: pageComponent

        Item {
            id:     root
            width:  availableWidth
            height: availableHeight

            property string sectionIdFilter: ""

            property real _margins:         ScreenTools.defaultFontPixelHeight / 2
            property var  _switchNameList:  [ "RC_MAP_ARM_SW", "RC_MAP_GEAR_SW", "RC_MAP_KILL_SW", "RC_MAP_LOITER_SW", "RC_MAP_OFFB_SW", "RC_MAP_RETURN_SW" ]
            property var  _switchTHList:    [ "RC_ARMSWITCH_TH", "RC_GEAR_TH", "RC_KILLSWITCH_TH", "RC_LOITER_TH", "RC_OFFB_TH", "RC_RETURN_TH" ]

            readonly property real  _channelComboWidth:     ScreenTools.defaultFontPixelWidth * 13
            readonly property real  _warningTextWidth:      ScreenTools.defaultFontPixelWidth * 40
            readonly property color _activeModeColor:       "yellow"
            readonly property var   _modeSelectionModel:    [ qsTr("Mode Channel"), qsTr("Buttons") ]

            property bool _buttonModeSupported:     buttonsController.supported
            property Fact _rcMapFltmodeFact:        controller.getParameterFact(-1, "RC_MAP_FLTMODE")
            property bool _buttonMode:              false
            property bool _slotsExhausted:          false
            property bool _channelChangeRefused:    false

            /// Channel whose button last selected a mode, 0 before any press.
            readonly property int _activeButtonChannel: (_buttonMode && buttonsController.activeSlot > 0 &&
                                                         buttonsController.activeSlot <= buttonsController.slotChannels.length) ?
                                                            buttonsController.slotChannels[buttonsController.activeSlot - 1] : 0

            Component.onCompleted: {
                if (controller.vehicle.vtol) {
                    _switchNameList.push("RC_MAP_TRANS_SW")
                    _switchTHList.push("RC_TRANS_TH")
                }
                if (controller.vehicle.fixedWing) {
                    _switchNameList.push("RC_MAP_FLAPS")
                    _switchTHList.push("")
                }
                switchRepeater.model = _switchNameList
                _buttonMode = _isButtonModeActive()
            }

            /// Mirrors the controller: PX4 honours the button bitmask only while no mode channel is mapped.
            function _isButtonModeActive() {
                if (!_buttonModeSupported || buttonsController.maskFact.rawValue <= 0) {
                    return false
                }
                return !_rcMapFltmodeFact || _rcMapFltmodeFact.rawValue === 0
            }

            function _isButtonChannel(channel) {
                return buttonsController.slotChannels.indexOf(channel) !== -1
            }

            /// Mode Channel and Buttons are mutually exclusive, so selecting one clears the other's parameter.
            function _selectButtonMode(buttonMode) {
                _slotsExhausted = false
                _channelChangeRefused = false
                if (buttonMode) {
                    if (_rcMapFltmodeFact) {
                        _rcMapFltmodeFact.rawValue = 0
                    }
                } else if (buttonsController.maskFact) {
                    buttonsController.maskFact.rawValue = 0
                }
                _buttonMode = buttonMode
            }

            PX4SimpleFlightModesController {
                id: controller
            }

            PX4FlightModeButtonsController {
                id: buttonsController
            }

            QGCPalette {
                id:                 qgcPalDisabled
                colorGroupEnabled:  false
            }

            // One-directional on purpose: a mask arriving from elsewhere means the vehicle is on buttons,
            // but unticking the last channel must not eject the user from the panel they are still editing.
            Connections {
                target: buttonsController.maskFact
                function onRawValueChanged() {
                    if (_isButtonModeActive()) {
                        _buttonMode = true
                    }
                }
            }

            // A mode channel appearing from elsewhere wins: PX4 ignores the bitmask while one is mapped.
            Connections {
                target: _rcMapFltmodeFact
                function onRawValueChanged() {
                    if (_rcMapFltmodeFact.rawValue !== 0) {
                        _buttonMode = false
                    }
                }
            }

            QGCFlickable {
                anchors.fill:   parent
                clip:           true
                contentWidth:   column2.x + column2.width
                contentHeight:  Math.max(column1.height, column2.height)

                Column {
                    id:         column1
                    spacing:    _margins

                    Row {
                        id:         settingsRow
                        spacing:    _margins

                        Column {
                            id:      flightModeSettingsColumn
                            spacing: _margins
                            visible: sectionIdFilter === "" || sectionIdFilter === "Flight Modes"

                            QGCLabel {
                                id:             flightModeLabel
                                text:           qsTr("Flight Mode Settings")
                                font.bold:      true
                            }

                            Rectangle {
                                id:                 flightModeSettings
                                width:              flightModeSettingsLayout.implicitWidth + (_margins * 2)
                                height:             flightModeSettingsLayout.implicitHeight + ScreenTools.defaultFontPixelHeight
                                color:              qgcPal.windowShade

                                ColumnLayout {
                                    id:                 flightModeSettingsLayout
                                    anchors.margins:    ScreenTools.defaultFontPixelWidth
                                    anchors.left:       parent.left
                                    anchors.top:        parent.top
                                    spacing:            ScreenTools.defaultFontPixelWidth / 2

                                    RowLayout {
                                        Layout.fillWidth:   true
                                        spacing:            ScreenTools.defaultFontPixelWidth
                                        visible:            _buttonModeSupported

                                        QGCLabel {
                                            Layout.fillWidth:   true
                                            text:               qsTr("Mode Selection")
                                        }

                                        QGCComboBox {
                                            model:              _modeSelectionModel
                                            currentIndex:       _buttonMode ? 1 : 0
                                            sizeToContents:     true

                                            onActivated: (index) => {
                                                _selectButtonMode(index === 1)
                                                currentIndex = Qt.binding(function() { return _buttonMode ? 1 : 0 })
                                            }
                                        }
                                    }

                                    Rectangle {
                                        Layout.fillWidth:       true
                                        Layout.preferredHeight: 1
                                        color:                  qgcPal.windowShadeDark
                                        visible:                _buttonModeSupported
                                    }

                                    GridLayout {
                                        id:                 flightModeColumn
                                        rows:               7
                                        rowSpacing:         ScreenTools.defaultFontPixelWidth / 2
                                        columnSpacing:      rowSpacing
                                        flow:               GridLayout.TopToBottom
                                        visible:            !_buttonMode

                                        QGCLabel {
                                            Layout.fillWidth:   true
                                            text:               qsTr("Mode Channel")
                                        }

                                        Repeater {
                                            model: 6

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                text:               qsTr("Flight Mode %1").arg(modelData + 1)
                                                color:              (controller.activeFlightMode - 1) == index ? _activeModeColor : qgcPal.text
                                            }
                                        }

                                        FactComboBox {
                                            Layout.fillWidth:   true
                                            fact:               controller.getParameterFact(-1, "RC_MAP_FLTMODE")
                                            indexModel:         false
                                            sizeToContents:     true
                                        }

                                        Repeater {
                                            model: 6

                                            FactComboBox {
                                                Layout.fillWidth:   true
                                                fact:               controller.getParameterFact(-1, "COM_FLTMODE" + (modelData + 1))
                                                indexModel:         false
                                                sizeToContents:     true
                                            }
                                        }
                                    }

                                    ColumnLayout {
                                        id:         buttonModeColumn
                                        spacing:    ScreenTools.defaultFontPixelWidth / 2
                                        visible:    _buttonMode

                                        QGCLabel {
                                            text:   qsTr("Button channels")
                                            color:  qgcPalDisabled.text
                                        }

                                        GridLayout {
                                            columns:        3
                                            rowSpacing:     ScreenTools.defaultFontPixelWidth / 2
                                            columnSpacing:  ScreenTools.defaultFontPixelWidth

                                            Repeater {
                                                model: buttonsController.maskChannelCount

                                                QGCCheckBox {
                                                    text:       qsTr("Channel %1").arg(modelData + 1)
                                                    checked:    _isButtonChannel(modelData + 1)
                                                    textColor:  (modelData + 1) === _activeButtonChannel ? _activeModeColor : qgcPal.text

                                                    onClicked: {
                                                        _slotsExhausted = checked && !buttonsController.canAddChannel()
                                                        _channelChangeRefused = !_slotsExhausted &&
                                                                !buttonsController.setChannelEnabled(modelData + 1, checked)
                                                        checked = Qt.binding(function() { return _isButtonChannel(modelData + 1) })
                                                    }
                                                }
                                            }
                                        }

                                        QGCLabel {
                                            Layout.maximumWidth:    _warningTextWidth
                                            wrapMode:               Text.WordWrap
                                            color:                  qgcPal.warningText
                                            visible:                _slotsExhausted
                                            text:                   qsTr("At most %1 mode buttons can be selected").arg(buttonsController.maxSlots)
                                        }

                                        QGCLabel {
                                            Layout.maximumWidth:    _warningTextWidth
                                            wrapMode:               Text.WordWrap
                                            color:                  qgcPal.warningText
                                            visible:                _channelChangeRefused
                                            text:                   qsTr("Could not change the button channels. The vehicle is missing the flight mode parameters.")
                                        }

                                        QGCLabel {
                                            Layout.maximumWidth:    _warningTextWidth
                                            wrapMode:               Text.WordWrap
                                            color:                  qgcPal.warningText
                                            visible:                buttonsController.slotChannels.length > buttonsController.maxSlots
                                            text:                   qsTr("%1 channels are selected. PX4 ignores everything past the first %2 - untick the extras.").arg(buttonsController.slotChannels.length).arg(buttonsController.maxSlots)
                                        }

                                        Rectangle {
                                            Layout.fillWidth:       true
                                            Layout.preferredHeight: 1
                                            color:                  qgcPal.windowShadeDark
                                        }

                                        QGCLabel {
                                            text:   qsTr("Mode assignment")
                                            color:  qgcPalDisabled.text
                                        }

                                        Repeater {
                                            model: buttonsController.slotChannels.slice(0, buttonsController.maxSlots)

                                            RowLayout {
                                                id:                 assignmentRow
                                                Layout.fillWidth:   true
                                                spacing:            ScreenTools.defaultFontPixelWidth

                                                property Fact modeFact:     controller.getParameterFact(-1, "COM_FLTMODE" + (index + 1))
                                                property bool unassigned:   modeFact ? modeFact.rawValue < 0 : true

                                                QGCLabel {
                                                    Layout.fillWidth:   true
                                                    text:               qsTr("Flight Mode %1 - Ch %2").arg(index + 1).arg(modelData)
                                                    color:              buttonsController.activeSlot === (index + 1) ?
                                                                            _activeModeColor :
                                                                            (assignmentRow.unassigned ? qgcPalDisabled.text : qgcPal.text)
                                                }

                                                FactComboBox {
                                                    fact:               assignmentRow.modeFact
                                                    indexModel:         false
                                                    sizeToContents:     true
                                                }
                                            }
                                        }
                                    }
                                }
                            } // Rectangle - Flight Modes
                        } // Column - Flight mode settings

                        Column {
                            id:         column2
                            spacing:    _margins
                            visible:    sectionIdFilter === "" || sectionIdFilter === "Switch Settings"

                            QGCLabel {
                                text:           qsTr("Switch Settings")
                                font.bold:      true
                            }

                            Rectangle {
                                id:     switchSettingsRect
                                width:  switchSettingsGrid.width + (_margins * 2)
                                height: switchSettingsGrid.height + ScreenTools.defaultFontPixelHeight
                                color:  qgcPal.windowShade

                                GridLayout {
                                    id:                 switchSettingsGrid
                                    anchors.margins:    ScreenTools.defaultFontPixelWidth
                                    anchors.left:       parent.left
                                    anchors.top:        parent.top
                                    columns:            2
                                    columnSpacing:      ScreenTools.defaultFontPixelWidth

                                    Repeater {
                                        id: switchRepeater

                                        RowLayout {
                                            spacing:            ScreenTools.defaultFontPixelWidth
                                            Layout.fillWidth:   true

                                            property string thFactName:     _switchTHList[index]
                                            property bool   thFactExists:   thFactName !== ""
                                            property Fact   swFact:         controller.getParameterFact(-1, modelData)
                                            property Fact   thFact:         thFactExists ? controller.getParameterFact(-1, thFactName) : null
                                            property real   thValue:        thFactExists ? thFact.rawValue : 0.5
                                            property real   thPWM:          1000 + (1000 * thValue)
                                            property int    swChannel:      swFact.rawValue - 1
                                            property bool   swActive:       swChannel < 0 ?
                                                                                false :
                                                                                (thValue >= 0 ?
                                                                                     (controller.rcChannelValues[swChannel] > thPWM) :
                                                                                     (controller.rcChannelValues[swChannel] <= thPWM))
                                            QGCLabel {
                                                text:               swFact.shortDescription
                                                Layout.fillWidth:   true
                                                color:              swActive ? "yellow" : qgcPal.text
                                            }

                                            FactComboBox {
                                                Layout.preferredWidth:  _channelComboWidth
                                                fact:                   swFact
                                                indexModel:             false
                                            }
                                        }
                                    }
                                }
                            } // Rectangle

                            RCChannelMonitor {
                                width:      switchSettingsRect.width
                                twoColumn:  true
                            }
                        } // Column - Switch settings
                    } // Row - Settings
                }
            }
        }
    }
}
