#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>

#include "modbus.h"
#include "solax.h"
#include "status_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "electrical_power_measurement_delegate.h"

using chip::app::Clusters::ElectricalPowerMeasurement::ElectricalPowerMeasurementDelegate;
using chip::app::Clusters::ElectricalPowerMeasurement::kAcOutputProfile;
using chip::app::Clusters::ElectricalPowerMeasurement::kBatteryProfile;
using chip::app::Clusters::ElectricalPowerMeasurement::kPvStringProfile;

static ElectricalPowerMeasurementDelegate ACOutputDelegate(kAcOutputProfile);
static ElectricalPowerMeasurementDelegate PV1Delegate(kPvStringProfile);
static ElectricalPowerMeasurementDelegate PV2Delegate(kPvStringProfile);
static ElectricalPowerMeasurementDelegate BatteryDelegate(kBatteryProfile);

static const char *TAG = "Main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;

using namespace chip::app::Clusters;

static uint16_t inverter_endpoint_id = 0;
static uint16_t ac_output_endpoint_id = 0;
static uint16_t pv1_endpoint_id = 0;
static uint16_t pv2_endpoint_id = 0;
static uint16_t battery_endpoint_id = 0;

// Standard semantic tag namespaces (Matter 1.4) used to label the sub-parts,
// so a controller can tell the two otherwise identical PV strings apart.
#define NAMESPACE_COMMON_NUMBER          0x07
#define NAMESPACE_ELECTRICAL_MEASUREMENT 0x0A
#define NAMESPACE_POWER_SOURCE           0x0F

#define TAG_NUMBER_ONE                   0x01
#define TAG_NUMBER_TWO                   0x02
#define TAG_ELECTRICAL_DC                0x00
#define TAG_ELECTRICAL_AC                0x01
#define TAG_POWER_SOURCE_SOLAR           0x02
#define TAG_POWER_SOURCE_BATTERY         0x03

// Power Source cluster enum values.
#define POWER_SOURCE_STATUS_ACTIVE       1
#define WIRED_CURRENT_TYPE_AC            0
#define BAT_CHARGE_LEVEL_OK              0
#define BAT_CHARGE_LEVEL_WARNING         1
#define BAT_CHARGE_LEVEL_CRITICAL        2
#define BAT_CHARGE_STATE_IS_CHARGING     1
#define BAT_CHARGE_STATE_IS_NOT_CHARGING 3
#define BAT_REPLACEABILITY_NOT_REPLACEABLE 1

#define BATTERY_WARNING_SOC_PERCENT      20
#define BATTERY_CRITICAL_SOC_PERCENT     10

// RS-485 bring-up tests. 0 = off (normal Solax polling), 1 = transmit one
// byte repeatedly, 2 = receive and log bytes, 3 = transmit a byte and listen
// for it (jumper TXD to RXD to bypass the transceiver).
#define MODBUS_LINK_TEST 0

#define ABORT_APP_ON_FAILURE(x, ...)               \
    do                                             \
    {                                              \
        if (!(unlikely(x)))                        \
        {                                          \
            __VA_ARGS__;                           \
            vTaskDelay(5000 / portTICK_PERIOD_MS); \
            abort();                               \
        }                                          \
    } while (0)

static bool is_commissioned()
{
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

static void open_commissioning_window_if_necessary()
{
    VerifyOrReturn(chip::Server::GetInstance().GetFabricTable().FabricCount() == 0);

    chip::CommissioningWindowManager &mgr = chip::Server::GetInstance().GetCommissioningWindowManager();
    VerifyOrReturn(mgr.IsCommissioningWindowOpen() == false);

    CHIP_ERROR err = mgr.OpenBasicCommissioningWindow(
        chip::System::Clock::Seconds16(300),
        chip::CommissioningWindowAdvertisement::kDnssdOnly);

    if (err != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to open commissioning window: %" CHIP_ERROR_FORMAT, err.Format());
    }
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type)
    {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        status_led_set_state(STATUS_LED_OPERATIONAL);
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        status_led_set_state(STATUS_LED_PAIRING);
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        // Also fires when a pairing attempt is abandoned or fails, in which
        // case the window is still open and the device is pairable again.
        ESP_LOGI(TAG, "Commissioning session stopped");
        status_led_set_state(is_commissioned() ? STATUS_LED_OPERATIONAL : STATUS_LED_READY_TO_PAIR);
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        status_led_set_state(is_commissioned() ? STATUS_LED_OPERATIONAL : STATUS_LED_OFF);
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
    {
        ESP_LOGI(TAG, "Commissioning window opened");

        status_led_set_state(STATUS_LED_READY_TO_PAIR);

        chip::RendezvousInformationFlags rendezvousFlags = chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE);

        chip::PayloadContents payload;
        GetPayloadContents(payload, rendezvousFlags);

        char payloadBuffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
        chip::MutableCharSpan qrCode(payloadBuffer);

        if (GetQRCode(qrCode, payload) == CHIP_NO_ERROR)
        {
            ESP_LOGI(TAG, "Generated QR CODE [%d]: %s", qrCode.size(), qrCode.data());
        }
        else
        {
            ESP_LOGE(TAG, "Failed to generate the commissioning QR code");
        }
        break;
    }
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        open_commissioning_window_if_necessary();
        break;
    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized");
        break;
    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id,
                                       uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                         uint16_t endpoint_id,
                                         uint32_t cluster_id,
                                         uint32_t attribute_id,
                                         esp_matter_attr_val_t *val,
                                         void *priv_data)
{
    return ESP_OK;
}

static chip::app::DataModel::Nullable<int64_t> milli(float value)
{
    return chip::app::DataModel::MakeNullable((int64_t)(value * 1000.0f));
}

static void update_battery_power_source(const solax_reading_t &r)
{
    esp_matter_attr_val_t val = esp_matter_nullable_uint32(nullable<uint32_t>((uint32_t)(r.battery_voltage_v * 1000.0f))); // V → mV
    attribute::update(battery_endpoint_id, PowerSource::Id, PowerSource::Attributes::BatVoltage::Id, &val);

    val = esp_matter_nullable_uint8(nullable<uint8_t>(r.battery_soc_percent * 2)); // % → half-percent
    attribute::update(battery_endpoint_id, PowerSource::Id, PowerSource::Attributes::BatPercentRemaining::Id, &val);

    uint8_t charge_level = BAT_CHARGE_LEVEL_OK;
    if (r.battery_soc_percent <= BATTERY_CRITICAL_SOC_PERCENT)
    {
        charge_level = BAT_CHARGE_LEVEL_CRITICAL;
    }
    else if (r.battery_soc_percent <= BATTERY_WARNING_SOC_PERCENT)
    {
        charge_level = BAT_CHARGE_LEVEL_WARNING;
    }
    val = esp_matter_enum8(charge_level);
    attribute::update(battery_endpoint_id, PowerSource::Id, PowerSource::Attributes::BatChargeLevel::Id, &val);

    val = esp_matter_enum8(r.battery_power_w > 0 ? BAT_CHARGE_STATE_IS_CHARGING : BAT_CHARGE_STATE_IS_NOT_CHARGING);
    attribute::update(battery_endpoint_id, PowerSource::Id, PowerSource::Attributes::BatChargeState::Id, &val);
}

// Runs on the Matter thread. The reading is heap-allocated by
// matter_update_readings because it is too large to capture in ScheduleLambda.
static void apply_readings(intptr_t arg)
{
    solax_reading_t *r = reinterpret_cast<solax_reading_t *>(arg);

    // Matter reports power flowing into an endpoint as positive, so anything
    // the inverter generates (AC output, PV strings) is negated. Solax already
    // reports battery power as positive while charging, which matches.
    ACOutputDelegate.SetVoltage(milli(r->grid_voltage_v));
    ACOutputDelegate.SetActiveCurrent(milli(-r->grid_current_a));
    ACOutputDelegate.SetActivePower(milli(-r->grid_power_w));

    PV1Delegate.SetVoltage(milli(r->pv1_voltage_v));
    PV1Delegate.SetActiveCurrent(milli(-r->pv1_current_a));
    PV1Delegate.SetActivePower(milli(-r->pv1_power_w));

    PV2Delegate.SetVoltage(milli(r->pv2_voltage_v));
    PV2Delegate.SetActiveCurrent(milli(-r->pv2_current_a));
    PV2Delegate.SetActivePower(milli(-r->pv2_power_w));

    BatteryDelegate.SetVoltage(milli(r->battery_voltage_v));
    BatteryDelegate.SetActiveCurrent(milli(r->battery_current_a));
    BatteryDelegate.SetActivePower(milli(r->battery_power_w));

    update_battery_power_source(*r);

    delete r;
}

void matter_update_readings(const solax_reading_t &reading)
{
    solax_reading_t *copy = new solax_reading_t(reading);
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(apply_readings, reinterpret_cast<intptr_t>(copy)) != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to schedule Matter update");
        delete copy;
    }
}

// Builds an Electrical Sensor sub-part of the inverter. Built by hand rather
// than with electrical_sensor::create(), which forces the Node topology
// feature; a sub-part only measures itself, so it uses Tree topology.
static endpoint_t *create_electrical_sensor_part(node_t *node, endpoint_t *parent,
                                                 ElectricalPowerMeasurementDelegate *delegate, uint32_t epm_feature)
{
    endpoint_t *part = endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(part != nullptr, ESP_LOGE(TAG, "Failed to create sub-part endpoint"));

    descriptor::config_t descriptor_config;
    cluster_t *descriptor_cluster = descriptor::create(part, &descriptor_config, CLUSTER_FLAG_SERVER);
    ABORT_APP_ON_FAILURE(descriptor_cluster != nullptr, ESP_LOGE(TAG, "Failed to create descriptor cluster"));
    descriptor::feature::taglist::add(descriptor_cluster);

    add_device_type(part, ESP_MATTER_ELECTRICAL_SENSOR_DEVICE_TYPE_ID, ESP_MATTER_ELECTRICAL_SENSOR_DEVICE_TYPE_VERSION);

    power_topology::config_t power_topology_config;
    power_topology_config.feature_flags = power_topology::feature::tree_topology::get_id();
    ABORT_APP_ON_FAILURE(power_topology::create(part, &power_topology_config, CLUSTER_FLAG_SERVER) != nullptr,
                         ESP_LOGE(TAG, "Failed to create power topology cluster"));

    electrical_power_measurement::config_t epm_config;
    epm_config.feature_flags = epm_feature;
    epm_config.delegate = delegate;
    cluster_t *epm_cluster = electrical_power_measurement::create(part, &epm_config, CLUSTER_FLAG_SERVER);
    ABORT_APP_ON_FAILURE(epm_cluster != nullptr, ESP_LOGE(TAG, "Failed to create EPM cluster"));

    electrical_power_measurement::attribute::create_voltage(epm_cluster, 0);
    electrical_power_measurement::attribute::create_active_current(epm_cluster, 0);

    ABORT_APP_ON_FAILURE(set_parent_endpoint(part, parent) == ESP_OK, ESP_LOGE(TAG, "Failed to parent sub-part"));

    return part;
}

static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type make_tag(uint8_t namespace_id, uint8_t tag,
                                                                                  const char *label = nullptr)
{
    chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type t;
    t.namespaceID = namespace_id;
    t.tag = tag;
    if (label)
    {
        t.label.SetValue(chip::app::DataModel::MakeNullable(chip::CharSpan::fromCharString(label)));
    }
    return t;
}

// The tag lists are referenced (not copied) by the data model, so they must
// outlive the endpoints.
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type ac_output_tags[2];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type pv1_tags[3];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type pv2_tags[3];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type battery_tags[2];

static void set_sub_part_tag_lists()
{
    ac_output_tags[0] = make_tag(NAMESPACE_ELECTRICAL_MEASUREMENT, TAG_ELECTRICAL_AC, "AC Output");
    ac_output_tags[1] = make_tag(NAMESPACE_POWER_SOURCE, TAG_POWER_SOURCE_SOLAR);

    pv1_tags[0] = make_tag(NAMESPACE_ELECTRICAL_MEASUREMENT, TAG_ELECTRICAL_DC, "String 1");
    pv1_tags[1] = make_tag(NAMESPACE_POWER_SOURCE, TAG_POWER_SOURCE_SOLAR);
    pv1_tags[2] = make_tag(NAMESPACE_COMMON_NUMBER, TAG_NUMBER_ONE);

    pv2_tags[0] = make_tag(NAMESPACE_ELECTRICAL_MEASUREMENT, TAG_ELECTRICAL_DC, "String 2");
    pv2_tags[1] = make_tag(NAMESPACE_POWER_SOURCE, TAG_POWER_SOURCE_SOLAR);
    pv2_tags[2] = make_tag(NAMESPACE_COMMON_NUMBER, TAG_NUMBER_TWO);

    battery_tags[0] = make_tag(NAMESPACE_ELECTRICAL_MEASUREMENT, TAG_ELECTRICAL_DC, "Battery");
    battery_tags[1] = make_tag(NAMESPACE_POWER_SOURCE, TAG_POWER_SOURCE_BATTERY);

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    ::SetTagList(ac_output_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(ac_output_tags));
    ::SetTagList(pv1_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(pv1_tags));
    ::SetTagList(pv2_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(pv2_tags));
    ::SetTagList(battery_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(battery_tags));
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

extern "C" void app_main()
{
    nvs_flash_init();

    status_led_init();

    modbus_uart_init();
    ESP_LOGI(TAG, "Modbus UART initialized");

    // Create the root endpoint
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    ABORT_APP_ON_FAILURE(node != nullptr, ESP_LOGE(TAG, "Failed to create Matter node"));

    // Inverter: the top-level Solar Power endpoint. It carries the Power
    // Source device type itself; the measurements live on its sub-parts.
    // solar_power::create() is not used because it puts an Electrical Sensor
    // on this endpoint instead of on separate sub-parts.
    endpoint_t *inverter = endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(inverter != nullptr, ESP_LOGE(TAG, "Failed to create inverter endpoint"));

    descriptor::config_t inverter_descriptor_config;
    ABORT_APP_ON_FAILURE(descriptor::create(inverter, &inverter_descriptor_config, CLUSTER_FLAG_SERVER) != nullptr,
                         ESP_LOGE(TAG, "Failed to create inverter descriptor cluster"));
    add_device_type(inverter, ESP_MATTER_SOLAR_POWER_DEVICE_TYPE_ID, ESP_MATTER_SOLAR_POWER_DEVICE_TYPE_VERSION);

    endpoint::power_source_device::config_t inverter_power_source_config;
    inverter_power_source_config.power_source.status = POWER_SOURCE_STATUS_ACTIVE;
    inverter_power_source_config.power_source.order = 0;
    strncpy(inverter_power_source_config.power_source.description, "Solar Inverter",
            sizeof(inverter_power_source_config.power_source.description) - 1);
    inverter_power_source_config.power_source.feature_flags = cluster::power_source::feature::wired::get_id();
    inverter_power_source_config.power_source.features.wired.wired_current_type = WIRED_CURRENT_TYPE_AC;
    ABORT_APP_ON_FAILURE(endpoint::power_source_device::add(inverter, &inverter_power_source_config) == ESP_OK,
                         ESP_LOGE(TAG, "Failed to add power source to inverter"));
    inverter_endpoint_id = endpoint::get_id(inverter);

    // Sub-parts: AC output, the two PV strings (MPPT inputs) and the battery.
    uint32_t ac = electrical_power_measurement::feature::alternating_current::get_id();
    uint32_t dc = electrical_power_measurement::feature::direct_current::get_id();

    ac_output_endpoint_id = endpoint::get_id(create_electrical_sensor_part(node, inverter, &ACOutputDelegate, ac));
    pv1_endpoint_id = endpoint::get_id(create_electrical_sensor_part(node, inverter, &PV1Delegate, dc));
    pv2_endpoint_id = endpoint::get_id(create_electrical_sensor_part(node, inverter, &PV2Delegate, dc));

    endpoint_t *battery = create_electrical_sensor_part(node, inverter, &BatteryDelegate, dc);
    battery_endpoint_id = endpoint::get_id(battery);

    endpoint::power_source_device::config_t battery_power_source_config;
    battery_power_source_config.power_source.status = POWER_SOURCE_STATUS_ACTIVE;
    battery_power_source_config.power_source.order = 1;
    strncpy(battery_power_source_config.power_source.description, "Battery",
            sizeof(battery_power_source_config.power_source.description) - 1);
    battery_power_source_config.power_source.feature_flags =
        cluster::power_source::feature::battery::get_id() | cluster::power_source::feature::rechargeable::get_id();
    battery_power_source_config.power_source.features.battery.bat_charge_level = BAT_CHARGE_LEVEL_OK;
    battery_power_source_config.power_source.features.battery.bat_replaceability = BAT_REPLACEABILITY_NOT_REPLACEABLE;
    battery_power_source_config.power_source.features.rechargeable.bat_charge_state = BAT_CHARGE_STATE_IS_NOT_CHARGING;
    battery_power_source_config.power_source.features.rechargeable.bat_functional_while_charging = true;
    ABORT_APP_ON_FAILURE(endpoint::power_source_device::add(battery, &battery_power_source_config) == ESP_OK,
                         ESP_LOGE(TAG, "Failed to add power source to battery"));

    cluster_t *battery_power_source = cluster::get(battery, PowerSource::Id);
    cluster::power_source::attribute::create_bat_voltage(battery_power_source, nullable<uint32_t>(), nullable<uint32_t>(0), nullable<uint32_t>(0xFFFFFFFE));
    cluster::power_source::attribute::create_bat_percent_remaining(battery_power_source, nullable<uint8_t>(), nullable<uint8_t>(0), nullable<uint8_t>(200));

    ESP_LOGI(TAG, "Inverter endpoint %u: AC output %u, PV1 %u, PV2 %u, battery %u", inverter_endpoint_id,
             ac_output_endpoint_id, pv1_endpoint_id, pv2_endpoint_id, battery_endpoint_id);

    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Matter: %d", err);
        return;
    }

    set_sub_part_tag_lists();

#if MODBUS_LINK_TEST == 1
    modbus_start_tx_test();
#elif MODBUS_LINK_TEST == 2
    modbus_start_rx_test();
#elif MODBUS_LINK_TEST == 3
    modbus_start_loopback_test();
#else
    xTaskCreate(solax_read_task, "solax_read", 4096, NULL, 5, NULL);
#endif
}
