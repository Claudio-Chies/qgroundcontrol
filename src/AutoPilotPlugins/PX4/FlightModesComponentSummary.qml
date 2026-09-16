import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.FactControls
import QGroundControl.Controls

Item {
    implicitWidth: mainLayout.implicitWidth
    implicitHeight: mainLayout.implicitHeight
    width: parent.width  // grows when Loader is wider than implicitWidth

    FactPanelController { id: controller; }

    property Fact _nullFact
    property Fact _rcMapFltmode:    controller.parameterExists(-1, "RC_MAP_FLTMODE") ? controller.getParameterFact(-1, "RC_MAP_FLTMODE") : _nullFact
    property Fact _rcMapFltmBtn:    controller.parameterExists(-1, "RC_MAP_FLTM_BTN") ? controller.getParameterFact(-1, "RC_MAP_FLTM_BTN") : _nullFact

    // PX4 honours the button bitmask only while no mode channel is mapped.
    property bool _buttonMode:      (_rcMapFltmBtn ? _rcMapFltmBtn.rawValue > 0 : false) &&
                                    (_rcMapFltmode ? _rcMapFltmode.rawValue === 0 : false)

	ColumnLayout {
		id: mainLayout
		width: parent.width
		spacing: 0

		VehicleSummaryRow {
			labelText: qsTr("Mode switch")
			valueText: _buttonMode ?
                           qsTr("Buttons") :
                           (_rcMapFltmode.value === 0 ? qsTr("Setup required") : _rcMapFltmode.enumStringValue)
		}
		Repeater {
			model: 6
			VehicleSummaryRow {
				labelText: qsTr("Flight Mode %1 ").arg(index + 1)
				valueText: controller.getParameterFact(-1, "COM_FLTMODE" + (index + 1)).enumStringValue
			}
		}
    }
}
