#include "PX4FlightModeButtonsControllerTest.h"

#include <QtTest/QTest>

#include "Fact.h"
#include "MockLink.h"
#include "PX4FlightModeButtonsController.h"
#include "ParameterManager.h"
#include "QGCMAVLink.h"

UT_REGISTER_TEST(PX4FlightModeButtonsControllerTest, TestLabel::Integration, TestLabel::Vehicle)

void PX4FlightModeButtonsControllerTest::_setMask(int mask)
{
    Fact* const maskFact = getFact(QStringLiteral("RC_MAP_FLTM_BTN"));
    QVERIFY(maskFact);
    maskFact->setRawValue(mask);
    _waitForWritesToReachVehicle();
}

void PX4FlightModeButtonsControllerTest::_setModeSlots(const QList<int>& modes)
{
    for (int slot = 1; slot <= modes.count(); slot++) {
        Fact* const modeFact = getFact(QStringLiteral("COM_FLTMODE%1").arg(slot));
        QVERIFY(modeFact);
        modeFact->setRawValue(modes[slot - 1]);
    }
    _waitForWritesToReachVehicle();
}

QList<int> PX4FlightModeButtonsControllerTest::_modeSlots() const
{
    QList<int> modes;

    for (int slot = 1; slot <= 6; slot++) {
        Fact* const modeFact = getFact(QStringLiteral("COM_FLTMODE%1").arg(slot));
        modes.append(modeFact ? modeFact->rawValue().toInt() : 0);
    }

    return modes;
}

void PX4FlightModeButtonsControllerTest::_waitForWritesToReachVehicle()
{
    // PARAM_SET runs on an asynchronous state machine. Let MockLink acknowledge every write before the
    // test ends, otherwise it fires during link teardown and its warnings fail strict log mode.
    QStringList names{QStringLiteral("RC_MAP_FLTM_BTN")};
    for (int slot = 1; slot <= 6; slot++) {
        names.append(QStringLiteral("COM_FLTMODE%1").arg(slot));
    }

    for (const QString& name : names) {
        Fact* const fact = getFact(name);
        QVERIFY(fact);
        QTRY_COMPARE_WITH_TIMEOUT(mockLink()->paramValue(MAV_COMP_ID_AUTOPILOT1, name).toInt(),
                                  fact->rawValue().toInt(), TestTimeout::mediumMs());
    }
    QVERIFY_TRUE_WAIT(!parameterManager()->pendingWrites(), TestTimeout::mediumMs());
}

void PX4FlightModeButtonsControllerTest::_maskChannelCountComesFromMetadata()
{
    PX4FlightModeButtonsController controller;

    QVERIFY(controller.property("supported").toBool());

    // Must not be derived from the live RC channel count, which is still zero until the first
    // RC_CHANNELS message arrives - the channel grid has to be usable before any stick moves.
    QCOMPARE(controller.maskChannelCount(), 18);
    QCOMPARE(controller.maxSlots(), 6);
}

void PX4FlightModeButtonsControllerTest::_slotChannelsFollowMask_data()
{
    QTest::addColumn<int>("mask");
    QTest::addColumn<QVariantList>("expectedChannels");

    QTest::newRow("empty") << 0 << QVariantList();
    QTest::newRow("channel 1") << 0x01 << QVariantList({1});
    QTest::newRow("channel 5") << 0x10 << QVariantList({5});
    QTest::newRow("ascending order") << 0x54 << QVariantList({3, 5, 7});
    QTest::newRow("six channels") << 0x3F << QVariantList({1, 2, 3, 4, 5, 6});
}

void PX4FlightModeButtonsControllerTest::_slotChannelsFollowMask()
{
    QFETCH(int, mask);
    QFETCH(QVariantList, expectedChannels);

    PX4FlightModeButtonsController controller;
    _setMask(mask);

    QCOMPARE(controller.property("slotChannels").toList(), expectedChannels);
}

void PX4FlightModeButtonsControllerTest::_enablingLowerChannelShiftsModesUp()
{
    PX4FlightModeButtonsController controller;

    _setMask(0x50);  // channels 5 and 7
    _setModeSlots({1, 2, -1, -1, -1, -1});

    // Channel 3 sorts below both, so it takes slot 1 and pushes the others up.
    QVERIFY(controller.setChannelEnabled(3, true));
    _waitForWritesToReachVehicle();

    QCOMPARE(controller.property("slotChannels").toList(), QVariantList({3, 5, 7}));
    QCOMPARE(_modeSlots(), QList<int>({-1, 1, 2, -1, -1, -1}));
}

void PX4FlightModeButtonsControllerTest::_disablingChannelClosesTheGap()
{
    PX4FlightModeButtonsController controller;

    _setMask(0x54);  // channels 3, 5 and 7
    _setModeSlots({1, 2, 3, -1, -1, -1});

    // Dropping the middle channel must leave channel 7 holding its own mode, not channel 5's.
    QVERIFY(controller.setChannelEnabled(5, false));
    _waitForWritesToReachVehicle();

    QCOMPARE(controller.property("slotChannels").toList(), QVariantList({3, 7}));
    QCOMPARE(_modeSlots(), QList<int>({1, 3, -1, -1, -1, -1}));
}

void PX4FlightModeButtonsControllerTest::_appendingChannelLeavesEarlierSlotsAlone()
{
    PX4FlightModeButtonsController controller;

    _setMask(0x05);  // channels 1 and 3
    _setModeSlots({1, 2, 4, -1, -1, -1});

    // Appending past the slots in use displaces nothing, so slot 3 keeps whatever was already there.
    QVERIFY(controller.setChannelEnabled(6, true));
    _waitForWritesToReachVehicle();

    QCOMPARE(controller.property("slotChannels").toList(), QVariantList({1, 3, 6}));
    QCOMPARE(_modeSlots(), QList<int>({1, 2, 4, -1, -1, -1}));
}

void PX4FlightModeButtonsControllerTest::_slotCapRefusesExtraChannel()
{
    PX4FlightModeButtonsController controller;

    _setMask(0x3F);  // channels 1 through 6
    _setModeSlots({1, 2, 3, 4, 5, 6});

    QVERIFY(!controller.canAddChannel());
    QVERIFY(!controller.setChannelEnabled(7, true));

    Fact* const maskFact = getFact(QStringLiteral("RC_MAP_FLTM_BTN"));
    QVERIFY(maskFact);
    QCOMPARE(maskFact->rawValue().toInt(), 0x3F);
    QCOMPARE(_modeSlots(), QList<int>({1, 2, 3, 4, 5, 6}));
}
