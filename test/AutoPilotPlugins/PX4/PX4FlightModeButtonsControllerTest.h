#pragma once

#include "ParameterTest.h"

class PX4FlightModeButtonsController;

/// Tests for the PX4 flight mode button controller: the RC_MAP_FLTM_BTN bitmask to mode slot mapping,
/// and the COM_FLTMODEn shifting which keeps each channel's mode assignment attached to that channel.
class PX4FlightModeButtonsControllerTest : public ParameterTest
{
    Q_OBJECT

private slots:
    void _maskChannelCountComesFromMetadata();
    void _slotChannelsFollowMask_data();
    void _slotChannelsFollowMask();
    void _enablingLowerChannelShiftsModesUp();
    void _disablingChannelClosesTheGap();
    void _appendingChannelLeavesEarlierSlotsAlone();
    void _slotCapRefusesExtraChannel();

private:
    void _setMask(int mask);
    void _setModeSlots(const QList<int> &modes);
    QList<int> _modeSlots() const;
};
