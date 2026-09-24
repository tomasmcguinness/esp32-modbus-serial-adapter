#pragma once

#include <app/clusters/electrical-power-measurement-server/electrical-power-measurement-server.h>
#include <lib/core/CHIPError.h>

namespace chip {
namespace app {
namespace Clusters {
namespace ElectricalPowerMeasurement {

// Describes what a single Electrical Sensor endpoint measures: AC or DC, and
// the accuracy and range reported for voltage, current and power.
struct MeasurementProfile
{
    PowerModeEnum powerMode;
    Span<const Structs::MeasurementAccuracyStruct::Type> accuracies;
};

extern const MeasurementProfile kAcOutputProfile;
extern const MeasurementProfile kPvStringProfile;
extern const MeasurementProfile kBatteryProfile;

class ElectricalPowerMeasurementDelegate : public Delegate
{
public:
    explicit ElectricalPowerMeasurementDelegate(const MeasurementProfile & profile) : mProfile(profile) {}
    ~ElectricalPowerMeasurementDelegate() = default;

    PowerModeEnum GetPowerMode() override { return mProfile.powerMode; }
    uint8_t GetNumberOfMeasurementTypes() override { return static_cast<uint8_t>(mProfile.accuracies.size()); }

    CHIP_ERROR StartAccuracyRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetAccuracyByIndex(uint8_t index, Structs::MeasurementAccuracyStruct::Type & accuracy) override;
    CHIP_ERROR EndAccuracyRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartRangesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetRangeByIndex(uint8_t, Structs::MeasurementRangeStruct::Type &) override { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndRangesRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicCurrentsByIndex(uint8_t, Structs::HarmonicMeasurementStruct::Type &) override { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndHarmonicCurrentsRead() override { return CHIP_NO_ERROR; }

    CHIP_ERROR StartHarmonicPhasesRead() override { return CHIP_NO_ERROR; }
    CHIP_ERROR GetHarmonicPhasesByIndex(uint8_t, Structs::HarmonicMeasurementStruct::Type &) override { return CHIP_ERROR_PROVIDER_LIST_EXHAUSTED; }
    CHIP_ERROR EndHarmonicPhasesRead() override { return CHIP_NO_ERROR; }

    DataModel::Nullable<int64_t> GetVoltage() override { return mVoltage; }
    DataModel::Nullable<int64_t> GetActiveCurrent() override { return mActiveCurrent; }
    DataModel::Nullable<int64_t> GetReactiveCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetApparentCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetActivePower() override { return mActivePower; }
    DataModel::Nullable<int64_t> GetReactivePower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetApparentPower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSVoltage() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSCurrent() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetRMSPower() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetFrequency() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetPowerFactor() override { return DataModel::NullNullable; }
    DataModel::Nullable<int64_t> GetNeutralCurrent() override { return DataModel::NullNullable; }

    CHIP_ERROR SetVoltage(DataModel::Nullable<int64_t> newValue);
    CHIP_ERROR SetActiveCurrent(DataModel::Nullable<int64_t> newValue);
    CHIP_ERROR SetActivePower(DataModel::Nullable<int64_t> newValue);

private:
    const MeasurementProfile & mProfile;
    DataModel::Nullable<int64_t> mVoltage;
    DataModel::Nullable<int64_t> mActiveCurrent;
    DataModel::Nullable<int64_t> mActivePower;
};

class ElectricalPowerMeasurementInstance : public Instance
{
public:
    ElectricalPowerMeasurementInstance(EndpointId aEndpointId, ElectricalPowerMeasurementDelegate & aDelegate,
                                       BitMask<Feature> aFeatures, BitMask<OptionalAttributes> aOptionalAttributes) :
        Instance(aEndpointId, aDelegate, aFeatures, aOptionalAttributes),
        mDelegate(&aDelegate)
    {}

    ElectricalPowerMeasurementInstance(const ElectricalPowerMeasurementInstance &)             = delete;
    ElectricalPowerMeasurementInstance & operator=(const ElectricalPowerMeasurementInstance &) = delete;

    CHIP_ERROR Init() { return Instance::Init(); }
    void Shutdown() { Instance::Shutdown(); }

    ElectricalPowerMeasurementDelegate * GetDelegate() { return mDelegate; }

private:
    ElectricalPowerMeasurementDelegate * mDelegate;
};

} // namespace ElectricalPowerMeasurement
} // namespace Clusters
} // namespace app
} // namespace chip
