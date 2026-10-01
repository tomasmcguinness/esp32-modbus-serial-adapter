#include "water_heater_delegates.h"
#include <app/reporting/reporting.h>
#include <esp_log.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using chip::Protocols::InteractionModel::Status;

static const char *TAG = "WaterHeater";

// ---------------------------------------------------------------------------
// Water Heater Management

namespace chip {
namespace app {
namespace Clusters {
namespace WaterHeaterManagement {

Status ReadOnlyWaterHeaterDelegate::HandleBoost(uint32_t, Optional<bool>, Optional<bool>, Optional<int16_t>, Optional<Percent>,
                                                Optional<Percent>)
{
    ESP_LOGW(TAG, "Rejected Boost; this adapter is read-only");
    return Status::Failure;
}

Status ReadOnlyWaterHeaterDelegate::HandleCancelBoost()
{
    ESP_LOGW(TAG, "Rejected CancelBoost; this adapter is read-only");
    return Status::Failure;
}

void ReadOnlyWaterHeaterDelegate::SetHeatDemand(bool heating)
{
    BitMask<WaterHeaterHeatSourceBitmap> demand;
    if (heating)
    {
        demand.Set(WaterHeaterHeatSourceBitmap::kHeatPump);
    }
    if (demand != mHeatDemand)
    {
        mHeatDemand = demand;
        MatterReportingAttributeChangeCallback(mEndpointId, WaterHeaterManagement::Id, Attributes::HeatDemand::Id);
    }
}

void ReadOnlyWaterHeaterDelegate::SetBoostState(bool active)
{
    BoostStateEnum state = active ? BoostStateEnum::kActive : BoostStateEnum::kInactive;
    if (state != mBoostState)
    {
        mBoostState = state;
        MatterReportingAttributeChangeCallback(mEndpointId, WaterHeaterManagement::Id, Attributes::BoostState::Id);
    }
}

} // namespace WaterHeaterManagement

// ---------------------------------------------------------------------------
// Water Heater Mode

namespace WaterHeaterMode {

struct ModeOption
{
    const char *label;
    uint8_t value;
    detail::Structs::ModeTagStruct::Type tag;
};

static ModeOption kModes[] = {
    { "Off", kModeOff, { .value = to_underlying(ModeTag::kOff) } },
    { "On", kModeManual, { .value = to_underlying(ModeTag::kManual) } },
    { "Scheduled", kModeTimed, { .value = to_underlying(ModeTag::kTimed) } },
};

CHIP_ERROR ReadOnlyWaterHeaterModeDelegate::GetModeLabelByIndex(uint8_t modeIndex, MutableCharSpan &label)
{
    VerifyOrReturnError(modeIndex < MATTER_ARRAY_SIZE(kModes), CHIP_ERROR_PROVIDER_LIST_EXHAUSTED);
    return CopyCharSpanToMutableCharSpan(CharSpan::fromCharString(kModes[modeIndex].label), label);
}

CHIP_ERROR ReadOnlyWaterHeaterModeDelegate::GetModeValueByIndex(uint8_t modeIndex, uint8_t &value)
{
    VerifyOrReturnError(modeIndex < MATTER_ARRAY_SIZE(kModes), CHIP_ERROR_PROVIDER_LIST_EXHAUSTED);
    value = kModes[modeIndex].value;
    return CHIP_NO_ERROR;
}

CHIP_ERROR ReadOnlyWaterHeaterModeDelegate::GetModeTagsByIndex(uint8_t modeIndex,
                                                               DataModel::List<detail::Structs::ModeTagStruct::Type> &tags)
{
    VerifyOrReturnError(modeIndex < MATTER_ARRAY_SIZE(kModes), CHIP_ERROR_PROVIDER_LIST_EXHAUSTED);
    VerifyOrReturnError(tags.size() >= 1, CHIP_ERROR_INVALID_ARGUMENT);
    tags[0] = kModes[modeIndex].tag;
    tags.reduce_size(1);
    return CHIP_NO_ERROR;
}

void ReadOnlyWaterHeaterModeDelegate::HandleChangeToMode(uint8_t newMode, ModeBase::Commands::ChangeToModeResponse::Type &response)
{
    ESP_LOGW(TAG, "Rejected ChangeToMode(%u); this adapter is read-only", newMode);
    response.status = to_underlying(ModeBase::StatusCode::kGenericFailure);
    response.statusText.SetValue(CharSpan::fromCharString("Read-only"));
}

void ReadOnlyWaterHeaterModeDelegate::SetCurrentMode(uint8_t mode)
{
    VerifyOrReturn(mInstance != nullptr);
    if (mInstance->GetCurrentMode() != mode && mInstance->UpdateCurrentMode(mode) != Status::Success)
    {
        ESP_LOGE(TAG, "Unknown DHW operation mode %u", mode);
    }
}

} // namespace WaterHeaterMode
} // namespace Clusters
} // namespace app
} // namespace chip
