#include "electrical_power_measurement_delegate.h"
#include <app/reporting/reporting.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::ElectricalPowerMeasurement;
using namespace chip::app::Clusters::ElectricalPowerMeasurement::Structs;

// Ranges are sized for the Solax X1 Hybrid G4 family. Values are signed:
// positive is power flowing into the endpoint, negative is power it produces.

#define ACCURACY_RANGE(min, max)                                                     \
    {                                                                                \
        .rangeMin = (min),                                                           \
        .rangeMax = (max),                                                           \
        .percentMax = chip::MakeOptional(static_cast<chip::Percent100ths>(1000)),    \
        .percentMin = chip::MakeOptional(static_cast<chip::Percent100ths>(100)),     \
        .percentTypical = chip::MakeOptional(static_cast<chip::Percent100ths>(500)), \
    }

#define ACCURACY(type, ranges)                                                        \
    {                                                                                 \
        .measurementType = (type),                                                    \
        .measured = true,                                                             \
        .minMeasuredValue = (ranges)[0].rangeMin,                                     \
        .maxMeasuredValue = (ranges)[0].rangeMax,                                     \
        .accuracyRanges = DataModel::List<const MeasurementAccuracyRangeStruct::Type>(ranges), \
    }

// AC output: 230V grid, up to ~6kW.
static const MeasurementAccuracyRangeStruct::Type acVoltageRanges[] = { ACCURACY_RANGE(0, 300'000) };          // 0-300V (mV)
static const MeasurementAccuracyRangeStruct::Type acCurrentRanges[] = { ACCURACY_RANGE(-40'000, 40'000) };     // +/-40A (mA)
static const MeasurementAccuracyRangeStruct::Type acPowerRanges[] = { ACCURACY_RANGE(-8'000'000, 8'000'000) }; // +/-8kW (mW)

// PV strings: up to 600V and 16A per MPPT input.
static const MeasurementAccuracyRangeStruct::Type pvVoltageRanges[] = { ACCURACY_RANGE(0, 600'000) };
static const MeasurementAccuracyRangeStruct::Type pvCurrentRanges[] = { ACCURACY_RANGE(-16'000, 0) };
static const MeasurementAccuracyRangeStruct::Type pvPowerRanges[] = { ACCURACY_RANGE(-10'000'000, 0) };

// Battery: high-voltage pack, charging (+) and discharging (-).
static const MeasurementAccuracyRangeStruct::Type batteryVoltageRanges[] = { ACCURACY_RANGE(0, 500'000) };
static const MeasurementAccuracyRangeStruct::Type batteryCurrentRanges[] = { ACCURACY_RANGE(-30'000, 30'000) };
static const MeasurementAccuracyRangeStruct::Type batteryPowerRanges[] = { ACCURACY_RANGE(-8'000'000, 8'000'000) };

static const MeasurementAccuracyStruct::Type kAcOutputAccuracies[] = {
    ACCURACY(MeasurementTypeEnum::kVoltage, acVoltageRanges),
    ACCURACY(MeasurementTypeEnum::kActiveCurrent, acCurrentRanges),
    ACCURACY(MeasurementTypeEnum::kActivePower, acPowerRanges),
};

static const MeasurementAccuracyStruct::Type kPvStringAccuracies[] = {
    ACCURACY(MeasurementTypeEnum::kVoltage, pvVoltageRanges),
    ACCURACY(MeasurementTypeEnum::kActiveCurrent, pvCurrentRanges),
    ACCURACY(MeasurementTypeEnum::kActivePower, pvPowerRanges),
};

static const MeasurementAccuracyStruct::Type kBatteryAccuracies[] = {
    ACCURACY(MeasurementTypeEnum::kVoltage, batteryVoltageRanges),
    ACCURACY(MeasurementTypeEnum::kActiveCurrent, batteryCurrentRanges),
    ACCURACY(MeasurementTypeEnum::kActivePower, batteryPowerRanges),
};

namespace chip {
namespace app {
namespace Clusters {
namespace ElectricalPowerMeasurement {

const MeasurementProfile kAcOutputProfile = { PowerModeEnum::kAc, Span<const MeasurementAccuracyStruct::Type>(kAcOutputAccuracies) };
const MeasurementProfile kPvStringProfile = { PowerModeEnum::kDc, Span<const MeasurementAccuracyStruct::Type>(kPvStringAccuracies) };
const MeasurementProfile kBatteryProfile = { PowerModeEnum::kDc, Span<const MeasurementAccuracyStruct::Type>(kBatteryAccuracies) };

} // namespace ElectricalPowerMeasurement
} // namespace Clusters
} // namespace app
} // namespace chip

CHIP_ERROR ElectricalPowerMeasurementDelegate::GetAccuracyByIndex(uint8_t index, MeasurementAccuracyStruct::Type &accuracy)
{
    if (index >= mProfile.accuracies.size())
    {
        return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED;
    }
    accuracy = mProfile.accuracies[index];
    return CHIP_NO_ERROR;
}

CHIP_ERROR ElectricalPowerMeasurementDelegate::SetVoltage(DataModel::Nullable<int64_t> newValue)
{
    DataModel::Nullable<int64_t> oldValue = mVoltage;
    mVoltage = newValue;
    if (oldValue != newValue)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, ElectricalPowerMeasurement::Id, Attributes::Voltage::Id);
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR ElectricalPowerMeasurementDelegate::SetActiveCurrent(DataModel::Nullable<int64_t> newValue)
{
    DataModel::Nullable<int64_t> oldValue = mActiveCurrent;
    mActiveCurrent = newValue;
    if (oldValue != newValue)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, ElectricalPowerMeasurement::Id, Attributes::ActiveCurrent::Id);
    }
    return CHIP_NO_ERROR;
}

CHIP_ERROR ElectricalPowerMeasurementDelegate::SetActivePower(DataModel::Nullable<int64_t> newValue)
{
    DataModel::Nullable<int64_t> oldValue = mActivePower;
    mActivePower = newValue;
    if (oldValue != newValue)
    {
        MatterReportingAttributeChangeCallback(mEndpointId, ElectricalPowerMeasurement::Id, Attributes::ActivePower::Id);
    }
    return CHIP_NO_ERROR;
}
