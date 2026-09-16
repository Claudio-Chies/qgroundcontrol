#include "PX4FlightModeButtonsController.h"
#include "Fact.h"
#include "ParameterManager.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

#include <QtCore/QStringList>

QGC_LOGGING_CATEGORY(PX4FlightModeButtonsControllerLog, "AutoPilotPlugins.PX4FlightModeButtonsController")

PX4FlightModeButtonsController::PX4FlightModeButtonsController(QObject *parent)
    : FactPanelController(parent)
    , _maskFact(parameterExists(-1, kMaskParamName) ? getParameterFact(-1, kMaskParamName) : nullptr)
    , _modeChannelFact(parameterExists(-1, kModeChannelParamName) ? getParameterFact(-1, kModeChannelParamName) : nullptr)
    , _supported(_maskFact != nullptr)
{
    if (!_supported || !_vehicle) {
        _supported = false;
        return;
    }

    QStringList usedParams;
    for (int slot = 1; slot <= kMaxSlots; slot++) {
        usedParams << QStringLiteral("COM_FLTMODE%1").arg(slot);
    }
    if (!_allParametersExists(ParameterManager::defaultComponentId, usedParams)) {
        _supported = false;
        return;
    }

    // The bitmask metadata carries one entry per addressable channel, so it is the authority on how
    // many checkboxes to offer and which bit each channel owns.
    const int metaDataChannelCount = _maskFact->bitmaskValues().count();
    if (metaDataChannelCount > 0) {
        _maskChannelCount = metaDataChannelCount;
    }

    _updateSlotChannels();

    (void) connect(_maskFact, &Fact::rawValueChanged, this, [this]() {
        _updateSlotChannels();
        _resetActiveSlot();
    });

    if (_modeChannelFact) {
        // Switching between the mode channel and buttons retires the slots the highlight refers to.
        (void) connect(_modeChannelFact, &Fact::rawValueChanged, this, &PX4FlightModeButtonsController::_resetActiveSlot);
    }

    (void) connect(_vehicle, &Vehicle::rcChannelsClampedChanged, this, &PX4FlightModeButtonsController::_channelValuesChanged);
}

bool PX4FlightModeButtonsController::_modeChannelMapped() const
{
    return _modeChannelFact && (_modeChannelFact->rawValue().toInt() != 0);
}

int PX4FlightModeButtonsController::_bitForChannel(int channel) const
{
    if (_maskFact) {
        const QVariantList bitmaskValues = _maskFact->bitmaskValues();
        if ((channel >= 1) && (channel <= bitmaskValues.count())) {
            return bitmaskValues[channel - 1].toInt();
        }
    }

    return 1 << (channel - 1);
}

void PX4FlightModeButtonsController::_updateSlotChannels()
{
    QVariantList slotChannels;

    if (_maskFact) {
        const int mask = _maskFact->rawValue().toInt();
        for (int channel = 1; channel <= _maskChannelCount; channel++) {
            if (mask & _bitForChannel(channel)) {
                slotChannels.append(channel);
            }
        }
    }

    if (slotChannels != _slotChannels) {
        _slotChannels = slotChannels;
        emit slotChannelsChanged();
    }
}

void PX4FlightModeButtonsController::_resetActiveSlot()
{
    if (_activeSlot != 0) {
        _activeSlot = 0;
        emit activeSlotChanged();
    }
}

int PX4FlightModeButtonsController::_slotForChannel(int mask, int channel) const
{
    int slot = 1;
    for (int lowerChannel = 1; lowerChannel < channel; lowerChannel++) {
        if (mask & _bitForChannel(lowerChannel)) {
            slot++;
        }
    }

    return slot;
}

Fact *PX4FlightModeButtonsController::_modeSlotFact(int slot) const
{
    return getParameterFact(-1, QStringLiteral("COM_FLTMODE%1").arg(slot), false);
}

bool PX4FlightModeButtonsController::_modeSlotsExist() const
{
    for (int slot = 1; slot <= kMaxSlots; slot++) {
        if (!_modeSlotFact(slot)) {
            qCWarning(PX4FlightModeButtonsControllerLog) << "COM_FLTMODE" << slot << "is missing, mode buttons cannot be configured";
            return false;
        }
    }

    return true;
}

bool PX4FlightModeButtonsController::_insertModeSlot(int slot)
{
    if ((slot < 1) || (slot > kMaxSlots) || !_modeSlotsExist()) {
        return false;
    }

    // Appending past the slots already in use displaces nothing, so the slot keeps whatever the mode
    // channel configuration left there. Inserting between them shifts the occupied slots up, which
    // duplicates one value: that copy is the slot to blank.
    const int occupied = qMin(static_cast<int>(_slotChannels.count()), kMaxSlots);
    if (slot > occupied) {
        return true;
    }

    for (int shiftTo = qMin(occupied + 1, kMaxSlots); shiftTo > slot; shiftTo--) {
        _modeSlotFact(shiftTo)->setRawValue(_modeSlotFact(shiftTo - 1)->rawValue());
    }
    _modeSlotFact(slot)->setRawValue(kUnassignedMode);

    return true;
}

bool PX4FlightModeButtonsController::_removeModeSlot(int slot)
{
    if (slot < 1) {
        return false;
    }
    if (slot > kMaxSlots) {
        // The channel sits past the last slot PX4 can drive, so no slot frees up.
        return true;
    }
    if (!_modeSlotsExist()) {
        return false;
    }

    const int filled = qMin(static_cast<int>(_slotChannels.count()), kMaxSlots);
    for (int shiftTo = slot; shiftTo < filled; shiftTo++) {
        _modeSlotFact(shiftTo)->setRawValue(_modeSlotFact(shiftTo + 1)->rawValue());
    }
    _modeSlotFact(filled)->setRawValue(kUnassignedMode);

    return true;
}

bool PX4FlightModeButtonsController::canAddChannel() const
{
    return _slotChannels.count() < kMaxSlots;
}

bool PX4FlightModeButtonsController::setChannelEnabled(int channel, bool enabled)
{
    if (!_maskFact || (channel < 1) || (channel > _maskChannelCount)) {
        return false;
    }

    const int bit = _bitForChannel(channel);
    int mask = _maskFact->rawValue().toInt();
    if (static_cast<bool>(mask & bit) == enabled) {
        return true;
    }

    const int slot = _slotForChannel(mask, channel);
    if (enabled) {
        if (!canAddChannel() || !_insertModeSlot(slot)) {
            return false;
        }
        mask |= bit;
    } else {
        if (!_removeModeSlot(slot)) {
            return false;
        }
        mask &= ~bit;
    }

    _maskFact->setRawValue(mask);

    return true;
}

bool PX4FlightModeButtonsController::_isButtonPressed(int channelIndex, int pwmValue) const
{
    Fact *minFact = getParameterFact(-1, QStringLiteral("RC%1_MIN").arg(channelIndex + 1), false);
    Fact *maxFact = getParameterFact(-1, QStringLiteral("RC%1_MAX").arg(channelIndex + 1), false);
    if (!minFact || !maxFact) {
        return false;
    }

    const int pwmMin = minFact->rawValue().toInt();
    const int pwmMax = maxFact->rawValue().toInt();
    if (pwmMax <= pwmMin) {
        return false;
    }

    // RCn_REV is deliberately ignored: applying it would make a resting button read as pressed on a
    // reversed channel, and a pressed one not register at all.
    return pwmValue > (pwmMin + ((pwmMax - pwmMin) * kButtonPressThreshold));
}

/// Connected to Vehicle::rcChannelsClampedChanged signal
void PX4FlightModeButtonsController::_channelValuesChanged(QVector<int> pwmValues)
{
    if (!_supported || _modeChannelMapped()) {
        return;
    }

    // A button latches its mode in the firmware, so the highlight stays on the last one pressed rather
    // than following the press itself. PX4 drives a mode slot only from the first kMaxSlots selected
    // channels; when several are held at once the highest slot wins.
    const int slotCount = qMin(static_cast<int>(_slotChannels.count()), kMaxSlots);

    int pressedSlot = 0;
    for (int slot = 0; slot < slotCount; slot++) {
        const int channelIndex = _slotChannels[slot].toInt() - 1;
        if ((channelIndex < 0) || (channelIndex >= pwmValues.size()) || (pwmValues[channelIndex] == -1)) {
            continue;
        }

        if (_isButtonPressed(channelIndex, pwmValues[channelIndex])) {
            pressedSlot = slot + 1;
        }
    }

    if ((pressedSlot == 0) || (pressedSlot == _activeSlot)) {
        return;
    }

    _activeSlot = pressedSlot;
    emit activeSlotChanged();
}
