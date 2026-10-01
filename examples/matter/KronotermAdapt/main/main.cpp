#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <nvs_flash.h>

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ESP32/OpenthreadLauncher.h>
#endif

#include "kronoterm.h"
#include "modbus.h"
#include "status_led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "electrical_power_measurement_delegate.h"
#include "water_heater_delegates.h"

using chip::app::Clusters::ElectricalPowerMeasurement::ElectricalPowerMeasurementDelegate;
using chip::app::Clusters::ElectricalPowerMeasurement::kHeatPumpProfile;
using chip::app::Clusters::WaterHeaterManagement::ReadOnlyWaterHeaterDelegate;
using chip::app::Clusters::WaterHeaterMode::ReadOnlyWaterHeaterModeDelegate;

static ElectricalPowerMeasurementDelegate HeatPumpDelegate(kHeatPumpProfile);
static ReadOnlyWaterHeaterDelegate WaterHeaterDelegate;
static ReadOnlyWaterHeaterModeDelegate WaterHeaterModeDelegate;

static const char *TAG = "Main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::cluster;
using namespace esp_matter::endpoint;

using namespace chip::app::Clusters;

static uint16_t heat_pump_endpoint_id = 0;
static uint16_t loop1_endpoint_id = 0;
static uint16_t outdoor_endpoint_id = 0;
static uint16_t flow_endpoint_id = 0;
static uint16_t return_endpoint_id = 0;
static uint16_t dhw_endpoint_id = 0;

// Standard semantic tag namespaces (Matter 1.4) used to label the sub-parts,
// so a controller can tell the three temperature sensors apart.
#define NAMESPACE_COMMON_DIRECTION       0x04
#define NAMESPACE_COMMON_LOCATION        0x06
#define NAMESPACE_COMMON_NUMBER          0x07

#define TAG_DIRECTION_FORWARD            0x04
#define TAG_DIRECTION_BACKWARD           0x05
#define TAG_LOCATION_OUTDOOR             0x01
#define TAG_NUMBER_ONE                   0x01

// Power Source cluster enum values.
#define POWER_SOURCE_STATUS_ACTIVE       1
#define WIRED_CURRENT_TYPE_AC            0

// Thermostat cluster enum and bitmap values.
#define THERMOSTAT_SYSTEM_MODE_OFF       0
#define THERMOSTAT_SYSTEM_MODE_COOL      3
#define THERMOSTAT_SYSTEM_MODE_HEAT      4
#define THERMOSTAT_SEQUENCE_HEATING_ONLY 2
#define THERMOSTAT_SEQUENCE_COOLING_AND_HEATING 4
#define THERMOSTAT_RUNNING_STATE_HEAT    0x0001
#define THERMOSTAT_RUNNING_STATE_COOL    0x0002

// Hot water setpoint limits, in 0.01 °C. Thermal disinfection heats the tank
// well above the Thermostat cluster's 30 °C default maximum.
#define DHW_MIN_SETPOINT                 1000
#define DHW_MAX_SETPOINT                 7500
#define DHW_DEFAULT_SETPOINT             5000

// RS-485 bring-up tests. 0 = off (normal Kronoterm polling), 1 = transmit one
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

// This adapter only mirrors the heat pump; it never writes to it. Rejecting
// controller writes to the thermostats (setpoints, system mode) keeps Matter
// from showing a value the heat pump never received. The firmware's own
// updates use attribute::report(), which does not come through here.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                         uint16_t endpoint_id,
                                         uint32_t cluster_id,
                                         uint32_t attribute_id,
                                         esp_matter_attr_val_t *val,
                                         void *priv_data)
{
    if (type == PRE_UPDATE && cluster_id == Thermostat::Id &&
        (endpoint_id == loop1_endpoint_id || endpoint_id == dhw_endpoint_id))
    {
        ESP_LOGW(TAG, "Rejected write to thermostat attribute 0x%04" PRIX32 "; this adapter is read-only", attribute_id);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static chip::app::DataModel::Nullable<int64_t> milli(float value)
{
    return chip::app::DataModel::MakeNullable((int64_t)(value * 1000.0f));
}

// Temperatures in Matter are in 0.01 °C.
static int16_t centi(float celsius)
{
    return (int16_t)(celsius * 100.0f + (celsius < 0 ? -0.5f : 0.5f));
}

static void report_temperature(uint16_t endpoint_id, float celsius)
{
    esp_matter_attr_val_t val = esp_matter_nullable_int16(nullable<int16_t>(centi(celsius)));
    attribute::report(endpoint_id, TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, &val);
}

static uint8_t loop1_system_mode(const kronoterm_reading_t &r)
{
    if (!r.system_on || !r.loop1_on)
    {
        return THERMOSTAT_SYSTEM_MODE_OFF;
    }
    switch (r.regime)
    {
    case KRONOTERM_REGIME_COOLING:
        return THERMOSTAT_SYSTEM_MODE_COOL;
    case KRONOTERM_REGIME_HEATING:
        return THERMOSTAT_SYSTEM_MODE_HEAT;
    default:
        return THERMOSTAT_SYSTEM_MODE_OFF;
    }
}

static uint16_t loop1_running_state(const kronoterm_reading_t &r)
{
    if (!r.loop1_pump_on)
    {
        return 0;
    }
    switch (r.working_function)
    {
    case KRONOTERM_FUNCTION_HEATING:
        return THERMOSTAT_RUNNING_STATE_HEAT;
    case KRONOTERM_FUNCTION_COOLING:
        return THERMOSTAT_RUNNING_STATE_COOL;
    default:
        return 0;
    }
}

static void update_loop1_thermostat(const kronoterm_reading_t &r)
{
    nullable<int16_t> local_temperature;
    if (r.loop1_room_temp_valid)
    {
        local_temperature = nullable<int16_t>(centi(r.loop1_room_temp_c));
    }
    esp_matter_attr_val_t val = esp_matter_nullable_int16(local_temperature);
    attribute::report(loop1_endpoint_id, Thermostat::Id, Thermostat::Attributes::LocalTemperature::Id, &val);

    val = esp_matter_int16(centi(r.loop1_room_setpoint_c));
    attribute::report(loop1_endpoint_id, Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &val);

    val = esp_matter_enum8(loop1_system_mode(r));
    attribute::report(loop1_endpoint_id, Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &val);

    val = esp_matter_bitmap16(loop1_running_state(r));
    attribute::report(loop1_endpoint_id, Thermostat::Id, Thermostat::Attributes::ThermostatRunningState::Id, &val);
}

static bool dhw_heating(const kronoterm_reading_t &r)
{
    return r.working_function == KRONOTERM_FUNCTION_DHW || r.working_function == KRONOTERM_FUNCTION_THERMAL_DISINFECTION;
}

static void update_hot_water(const kronoterm_reading_t &r)
{
    esp_matter_attr_val_t val = esp_matter_nullable_int16(nullable<int16_t>(centi(r.dhw_temp_c)));
    attribute::report(dhw_endpoint_id, Thermostat::Id, Thermostat::Attributes::LocalTemperature::Id, &val);

    val = esp_matter_int16(centi(r.dhw_setpoint_c));
    attribute::report(dhw_endpoint_id, Thermostat::Id, Thermostat::Attributes::OccupiedHeatingSetpoint::Id, &val);

    bool dhw_on = r.system_on && r.dhw_operation != chip::app::Clusters::WaterHeaterMode::kModeOff;
    val = esp_matter_enum8(dhw_on ? THERMOSTAT_SYSTEM_MODE_HEAT : THERMOSTAT_SYSTEM_MODE_OFF);
    attribute::report(dhw_endpoint_id, Thermostat::Id, Thermostat::Attributes::SystemMode::Id, &val);

    val = esp_matter_bitmap16(dhw_heating(r) ? THERMOSTAT_RUNNING_STATE_HEAT : 0);
    attribute::report(dhw_endpoint_id, Thermostat::Id, Thermostat::Attributes::ThermostatRunningState::Id, &val);

    WaterHeaterDelegate.SetHeatDemand(dhw_heating(r));
    WaterHeaterDelegate.SetBoostState(r.forced_dhw);
    WaterHeaterModeDelegate.SetCurrentMode(r.dhw_operation);
}

// Runs on the Matter thread. The reading is heap-allocated by
// matter_update_readings because it is too large to capture in ScheduleLambda.
static void apply_readings(intptr_t arg)
{
    kronoterm_reading_t *r = reinterpret_cast<kronoterm_reading_t *>(arg);

    // Power the heat pump draws flows into the endpoint, so it is positive.
    HeatPumpDelegate.SetActivePower(milli(r->power_w));

    update_loop1_thermostat(*r);
    update_hot_water(*r);

    report_temperature(outdoor_endpoint_id, r->outdoor_temp_c);
    report_temperature(flow_endpoint_id, r->flow_temp_c);
    report_temperature(return_endpoint_id, r->return_temp_c);

    delete r;
}

void matter_update_readings(const kronoterm_reading_t &reading)
{
    kronoterm_reading_t *copy = new kronoterm_reading_t(reading);
    if (chip::DeviceLayer::PlatformMgr().ScheduleWork(apply_readings, reinterpret_cast<intptr_t>(copy)) != CHIP_NO_ERROR)
    {
        ESP_LOGE(TAG, "Failed to schedule Matter update");
        delete copy;
    }
}

static void add_tag_list_feature(endpoint_t *endpoint)
{
    cluster_t *descriptor_cluster = cluster::get(endpoint, Descriptor::Id);
    ABORT_APP_ON_FAILURE(descriptor_cluster != nullptr, ESP_LOGE(TAG, "Sub-part has no descriptor cluster"));
    descriptor::feature::taglist::add(descriptor_cluster);
}

static endpoint_t *create_temperature_sensor_part(node_t *node, endpoint_t *parent)
{
    temperature_sensor::config_t config;
    endpoint_t *part = temperature_sensor::create(node, &config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(part != nullptr, ESP_LOGE(TAG, "Failed to create temperature sensor endpoint"));

    add_tag_list_feature(part);
    ABORT_APP_ON_FAILURE(set_parent_endpoint(part, parent) == ESP_OK, ESP_LOGE(TAG, "Failed to parent sub-part"));

    return part;
}

static endpoint_t *create_thermostat_part(node_t *node, endpoint_t *parent)
{
    endpoint::thermostat::config_t config;
    config.thermostat.feature_flags =
        cluster::thermostat::feature::heating::get_id() | cluster::thermostat::feature::cooling::get_id();
    config.thermostat.control_sequence_of_operation = THERMOSTAT_SEQUENCE_COOLING_AND_HEATING;
    config.thermostat.system_mode = THERMOSTAT_SYSTEM_MODE_OFF;

    endpoint_t *part = endpoint::thermostat::create(node, &config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(part != nullptr, ESP_LOGE(TAG, "Failed to create thermostat endpoint"));

    cluster_t *thermostat_cluster = cluster::get(part, Thermostat::Id);
    cluster::thermostat::attribute::create_thermostat_running_state(thermostat_cluster, 0);

    add_tag_list_feature(part);
    ABORT_APP_ON_FAILURE(set_parent_endpoint(part, parent) == ESP_OK, ESP_LOGE(TAG, "Failed to parent sub-part"));

    return part;
}

// Hot water tank. The Water Heater device type is a Thermostat (heating only)
// plus Water Heater Management and Water Heater Mode, both backed by
// read-only delegates.
static endpoint_t *create_water_heater_part(node_t *node, endpoint_t *parent)
{
    water_heater::config_t config;
    config.thermostat.feature_flags = cluster::thermostat::feature::heating::get_id();
    config.thermostat.features.heating.occupied_heating_setpoint = DHW_DEFAULT_SETPOINT;
    config.thermostat.control_sequence_of_operation = THERMOSTAT_SEQUENCE_HEATING_ONLY;
    config.thermostat.system_mode = THERMOSTAT_SYSTEM_MODE_OFF;
    config.water_heater_management.heater_types =
        chip::to_underlying(WaterHeaterManagement::WaterHeaterHeatSourceBitmap::kHeatPump);
    config.water_heater_management.delegate = &WaterHeaterDelegate;
    config.water_heater_mode.current_mode = WaterHeaterMode::kModeManual;
    config.water_heater_mode.delegate = &WaterHeaterModeDelegate;

    endpoint_t *part = water_heater::create(node, &config, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(part != nullptr, ESP_LOGE(TAG, "Failed to create water heater endpoint"));

    cluster_t *thermostat_cluster = cluster::get(part, Thermostat::Id);
    cluster::thermostat::attribute::create_thermostat_running_state(thermostat_cluster, 0);
    cluster::thermostat::attribute::create_abs_min_heat_setpoint_limit(thermostat_cluster, DHW_MIN_SETPOINT);
    cluster::thermostat::attribute::create_abs_max_heat_setpoint_limit(thermostat_cluster, DHW_MAX_SETPOINT);
    cluster::thermostat::attribute::create_min_heat_setpoint_limit(thermostat_cluster, DHW_MIN_SETPOINT);
    cluster::thermostat::attribute::create_max_heat_setpoint_limit(thermostat_cluster, DHW_MAX_SETPOINT);

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
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type loop1_tags[1];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type outdoor_tags[1];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type flow_tags[1];
static chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type return_tags[1];

static void set_sub_part_tag_lists()
{
    loop1_tags[0] = make_tag(NAMESPACE_COMMON_NUMBER, TAG_NUMBER_ONE, "Loop 1");
    outdoor_tags[0] = make_tag(NAMESPACE_COMMON_LOCATION, TAG_LOCATION_OUTDOOR, "Outdoor");
    flow_tags[0] = make_tag(NAMESPACE_COMMON_DIRECTION, TAG_DIRECTION_FORWARD, "Flow");
    return_tags[0] = make_tag(NAMESPACE_COMMON_DIRECTION, TAG_DIRECTION_BACKWARD, "Return");

    chip::DeviceLayer::PlatformMgr().LockChipStack();
    ::SetTagList(loop1_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(loop1_tags));
    ::SetTagList(outdoor_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(outdoor_tags));
    ::SetTagList(flow_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(flow_tags));
    ::SetTagList(return_endpoint_id, chip::Span<const chip::app::Clusters::Descriptor::Structs::SemanticTagStruct::Type>(return_tags));
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

    // Heat pump: the top-level endpoint. It carries the Heat Pump, Power
    // Source and Electrical Sensor device types. heat_pump::create() is not
    // used because it also adds Device Energy Management and Electrical Energy
    // Measurement, which a read-only adapter has nothing to drive.
    endpoint_t *heat_pump = endpoint::create(node, ENDPOINT_FLAG_NONE, NULL);
    ABORT_APP_ON_FAILURE(heat_pump != nullptr, ESP_LOGE(TAG, "Failed to create heat pump endpoint"));

    descriptor::config_t heat_pump_descriptor_config;
    ABORT_APP_ON_FAILURE(descriptor::create(heat_pump, &heat_pump_descriptor_config, CLUSTER_FLAG_SERVER) != nullptr,
                         ESP_LOGE(TAG, "Failed to create heat pump descriptor cluster"));
    add_device_type(heat_pump, ESP_MATTER_HEAT_PUMP_DEVICE_TYPE_ID, ESP_MATTER_HEAT_PUMP_DEVICE_TYPE_VERSION);

    endpoint::power_source_device::config_t power_source_config;
    power_source_config.power_source.status = POWER_SOURCE_STATUS_ACTIVE;
    power_source_config.power_source.order = 0;
    strncpy(power_source_config.power_source.description, "Mains",
            sizeof(power_source_config.power_source.description) - 1);
    power_source_config.power_source.feature_flags = cluster::power_source::feature::wired::get_id();
    power_source_config.power_source.features.wired.wired_current_type = WIRED_CURRENT_TYPE_AC;
    ABORT_APP_ON_FAILURE(endpoint::power_source_device::add(heat_pump, &power_source_config) == ESP_OK,
                         ESP_LOGE(TAG, "Failed to add power source to heat pump"));

    // Electrical Sensor built by hand, like the Solax example's sub-parts. The
    // heat pump measures the whole node, so it uses Node topology. Only active
    // power is available, so Voltage and ActiveCurrent are not created.
    add_device_type(heat_pump, ESP_MATTER_ELECTRICAL_SENSOR_DEVICE_TYPE_ID, ESP_MATTER_ELECTRICAL_SENSOR_DEVICE_TYPE_VERSION);

    power_topology::config_t power_topology_config;
    power_topology_config.feature_flags = power_topology::feature::node_topology::get_id();
    ABORT_APP_ON_FAILURE(power_topology::create(heat_pump, &power_topology_config, CLUSTER_FLAG_SERVER) != nullptr,
                         ESP_LOGE(TAG, "Failed to create power topology cluster"));

    electrical_power_measurement::config_t epm_config;
    epm_config.feature_flags = electrical_power_measurement::feature::alternating_current::get_id();
    epm_config.delegate = &HeatPumpDelegate;
    ABORT_APP_ON_FAILURE(electrical_power_measurement::create(heat_pump, &epm_config, CLUSTER_FLAG_SERVER) != nullptr,
                         ESP_LOGE(TAG, "Failed to create EPM cluster"));

    heat_pump_endpoint_id = endpoint::get_id(heat_pump);

    // Sub-parts: heating loop 1, the outdoor, flow and return sensors and the
    // hot water tank.
    loop1_endpoint_id = endpoint::get_id(create_thermostat_part(node, heat_pump));
    outdoor_endpoint_id = endpoint::get_id(create_temperature_sensor_part(node, heat_pump));
    flow_endpoint_id = endpoint::get_id(create_temperature_sensor_part(node, heat_pump));
    return_endpoint_id = endpoint::get_id(create_temperature_sensor_part(node, heat_pump));
    dhw_endpoint_id = endpoint::get_id(create_water_heater_part(node, heat_pump));

    ESP_LOGI(TAG, "Heat pump endpoint %u: loop 1 %u, outdoor %u, flow %u, return %u, hot water %u",
             heat_pump_endpoint_id, loop1_endpoint_id, outdoor_endpoint_id, flow_endpoint_id, return_endpoint_id,
             dhw_endpoint_id);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    // Thread runs on the ESP32-C6's own 802.15.4 radio.
    esp_openthread_platform_config_t openthread_config = {
        .radio_config = {.radio_mode = RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = HOST_CONNECTION_MODE_NONE},
        .port_config = {.storage_partition_name = "nvs", .netif_queue_size = 10, .task_queue_size = 10},
    };
    set_openthread_platform_config(&openthread_config);
#endif

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
    xTaskCreate(kronoterm_read_task, "kronoterm_read", 4096, NULL, 5, NULL);
#endif
}
