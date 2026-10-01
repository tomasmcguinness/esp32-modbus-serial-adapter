#pragma once

#include <app/clusters/mode-base-server/mode-base-server.h>
#include <app/clusters/water-heater-management-server/water-heater-management-server.h>

// Read-only delegates for the hot water (Water Heater) sub-part. They mirror
// the Kronoterm DHW state; commands from controllers (Boost, ChangeToMode)
// are rejected because the adapter never writes to the heat pump.

namespace chip {
namespace app {
namespace Clusters {
namespace WaterHeaterManagement {

class ReadOnlyWaterHeaterDelegate : public Delegate
{
public:
    Protocols::InteractionModel::Status HandleBoost(uint32_t duration, Optional<bool> oneShot, Optional<bool> emergencyBoost,
                                                    Optional<int16_t> temporarySetpoint, Optional<Percent> targetPercentage,
                                                    Optional<Percent> targetReheat) override;
    Protocols::InteractionModel::Status HandleCancelBoost() override;

    BitMask<WaterHeaterHeatSourceBitmap> GetHeaterTypes() override { return WaterHeaterHeatSourceBitmap::kHeatPump; }
    BitMask<WaterHeaterHeatSourceBitmap> GetHeatDemand() override { return mHeatDemand; }
    uint16_t GetTankVolume() override { return 0; }
    Energy_mWh GetEstimatedHeatRequired() override { return 0; }
    Percent GetTankPercentage() override { return 0; }
    BoostStateEnum GetBoostState() override { return mBoostState; }

    // Called from the Matter thread with each new reading.
    void SetHeatDemand(bool heating);
    void SetBoostState(bool active);

private:
    BitMask<WaterHeaterHeatSourceBitmap> mHeatDemand;
    BoostStateEnum mBoostState = BoostStateEnum::kInactive;
};

} // namespace WaterHeaterManagement

namespace WaterHeaterMode {

// Mode values, matching Kronoterm register 2026 (DHW operation).
constexpr uint8_t kModeOff = 0;
constexpr uint8_t kModeManual = 1;
constexpr uint8_t kModeTimed = 2;

class ReadOnlyWaterHeaterModeDelegate : public ModeBase::Delegate
{
public:
    CHIP_ERROR Init() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetModeLabelByIndex(uint8_t modeIndex, MutableCharSpan & label) override;
    CHIP_ERROR GetModeValueByIndex(uint8_t modeIndex, uint8_t & value) override;
    CHIP_ERROR GetModeTagsByIndex(uint8_t modeIndex, DataModel::List<detail::Structs::ModeTagStruct::Type> & tags) override;
    void HandleChangeToMode(uint8_t newMode, ModeBase::Commands::ChangeToModeResponse::Type & response) override;

    // Called from the Matter thread with each new reading.
    void SetCurrentMode(uint8_t mode);
};

} // namespace WaterHeaterMode
} // namespace Clusters
} // namespace app
} // namespace chip
