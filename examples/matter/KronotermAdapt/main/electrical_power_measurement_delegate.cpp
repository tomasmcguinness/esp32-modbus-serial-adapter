#include "electrical_power_measurement_delegate.h"
#include <app/reporting/reporting.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::DataModel;
using namespace chip::app::Clusters;
using namespace chip::app::Clusters::ElectricalPowerMeasurement;
using namespace chip::app::Clusters::ElectricalPowerMeasurement::Structs;

// Ranges are sized for the Kronoterm Adapt 0312. The heat pump only reports
// the electrical power it draws, so active power is the only measurement.
// Power is positive while the heat pump consumes it.

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

// Electrical input: up to ~6kW including the backup heater.
static const MeasurementAccuracyRangeStruct::Type powerRanges[] = { ACCURACY_RANGE(0, 6'000'000) }; // 0-6kW (mW)

static const MeasurementAccuracyStruct::Type kHeatPumpAccuracies[] = {
    ACCURACY(MeasurementTypeEnum::kActivePower, powerRanges),
};

namespace chip {
namespace app {
namespace Clusters {
namespace ElectricalPowerMeasurement {

const MeasurementProfile kHeatPumpProfile = { PowerModeEnum::kAc, Span<const MeasurementAccuracyStruct::Type>(kHeatPumpAccuracies) };

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
