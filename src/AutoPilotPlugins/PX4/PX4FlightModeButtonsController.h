#pragma once

#include <QtCore/QVariantList>
#include <QtCore/QVector>
#include <QtQmlIntegration/QtQmlIntegration>

#include "FactPanelController.h"

/// \brief MVC Controller for the flight mode buttons section of PX4FlightModes.qml
///
/// PX4 can select flight modes from momentary buttons instead of a single mode channel. RC_MAP_FLTM_BTN
/// is a bitmask of the channels which act as buttons; the selected channels fill COM_FLTMODE1..6 in
/// ascending channel order. The bitmask only applies while RC_MAP_FLTMODE is unmapped.
class PX4FlightModeButtonsController : public FactPanelController
{
    Q_OBJECT
    QML_ELEMENT
    Q_MOC_INCLUDE("Fact.h")

    Q_PROPERTY(bool         supported           MEMBER _supported           CONSTANT)
    Q_PROPERTY(Fact        *maskFact            MEMBER _maskFact            CONSTANT)
    Q_PROPERTY(int          maskChannelCount    READ maskChannelCount       CONSTANT)
    Q_PROPERTY(int          maxSlots            READ maxSlots               CONSTANT)
    Q_PROPERTY(QVariantList slotChannels        MEMBER _slotChannels        NOTIFY slotChannelsChanged)
    Q_PROPERTY(int          activeSlot          MEMBER _activeSlot          NOTIFY activeSlotChanged)

public:
    explicit PX4FlightModeButtonsController(QObject *parent = nullptr);

    /// Number of channels the bitmask can address, taken from the parameter's own metadata.
    /// Deliberately not channelCount, which is zero until the first RC_CHANNELS message arrives.
    int maskChannelCount() const { return _maskChannelCount; }
    int maxSlots() const { return kMaxSlots; }

    /// Adds or removes a channel from the mask, keeping each remaining channel's mode assignment.
    ///     @return true: mask updated, false: refused (slots exhausted or mode parameters missing)
    Q_INVOKABLE bool setChannelEnabled(int channel, bool enabled);

    Q_INVOKABLE bool canAddChannel() const;

signals:
    void slotChannelsChanged();
    void activeSlotChanged();

private slots:
    void _channelValuesChanged(QVector<int> pwmValues);

private:
    /// PX4 honours the bitmask only while no mode channel is mapped.
    bool _modeChannelMapped() const;
    int _bitForChannel(int channel) const;
    int _slotForChannel(int mask, int channel) const;
    Fact *_modeSlotFact(int slot) const;
    bool _modeSlotsExist() const;
    bool _insertModeSlot(int slot);
    bool _removeModeSlot(int slot);
    void _updateSlotChannels();
    void _resetActiveSlot();
    bool _isButtonPressed(int channelIndex, int pwmValue) const;

    Fact *_maskFact = nullptr;
    Fact *_modeChannelFact = nullptr;
    bool _supported = false;
    int _maskChannelCount = kDefaultMaskChannelCount;
    QVariantList _slotChannels;
    int _activeSlot = 0;

    static constexpr int kMaxSlots = 6;
    static constexpr int kUnassignedMode = -1;
    static constexpr int kDefaultMaskChannelCount = 18;
    static constexpr float kButtonPressThreshold = 0.5f;

    static constexpr const char *kMaskParamName = "RC_MAP_FLTM_BTN";
    static constexpr const char *kModeChannelParamName = "RC_MAP_FLTMODE";
};
