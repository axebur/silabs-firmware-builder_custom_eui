/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app/framework/include/af.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT
#include "network_test_config.h"
#endif //SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT

#if (LARGE_NETWORK_TESTING == 0)
#ifndef EZSP_HOST

#include "zigbee_sleep_config.h"
#endif

#include "btl_interface.h"
#include "network-steering.h"
#include "find-and-bind-target.h"
#include "sl_cli_handles.h"
#include "sl_token_api.h"
#include <string.h>

#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
#include "zll-commissioning.h"
#endif //SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT

#if defined(SL_CATALOG_LED0_PRESENT)
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#define led_turn_on(led) sl_led_turn_on(led)
#define led_turn_off(led) sl_led_turn_off(led)
#define led_toggle(led) sl_led_toggle(led)
#define COMMISSIONING_STATUS_LED (&sl_led_led0)
#define ON_OFF_LIGHT_LED         (&sl_led_led0)
#else // !SL_CATALOG_LED0_PRESENT
#define led_turn_on(led)
#define led_turn_off(led)
#define led_toggle(led)
#endif // SL_CATALOG_LED0_PRESENT

#define LED_BLINK_PERIOD_MS      500
#define LIGHT_ENDPOINT           1
#define STEERING_DELAY_MS        15000

static sl_zigbee_af_event_t commissioning_led_event;
static sl_zigbee_af_event_t finding_and_binding_event;

//---------------
// Custom EUI64 helpers

static bool is_valid_custom_eui64(const sl_802154_long_addr_t eui64)
{
  bool all_ff = true;
  bool all_00 = true;

  for (size_t i = 0; i < EUI64_SIZE; i++) {
    if (eui64[i] != 0xFF) {
      all_ff = false;
    }
    if (eui64[i] != 0x00) {
      all_00 = false;
    }
  }

  return !all_ff && !all_00;
}

static void print_eui64_be(const char *label, const sl_802154_long_addr_t eui64)
{
  printf("%s: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
         label,
         eui64[7], eui64[6], eui64[5], eui64[4],
         eui64[3], eui64[2], eui64[1], eui64[0]);
}

static int hex_nibble(char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static bool parse_eui64_be_str(const char *str, sl_802154_long_addr_t out_le)
{
  if (str == NULL) {
    return false;
  }

  if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
    str += 2;
  }

  uint8_t bytes_be[EUI64_SIZE];
  size_t byte_idx = 0;

  while (*str != '\0') {
    if (*str == ':' || *str == '-') {
      str++;
      continue;
    }

    if (byte_idx >= EUI64_SIZE) {
      return false;
    }

    int hi = hex_nibble(str[0]);
    int lo = hex_nibble(str[1]);
    if (hi < 0 || lo < 0) {
      return false;
    }

    bytes_be[byte_idx++] = (uint8_t)((hi << 4) | lo);
    str += 2;
  }

  if (byte_idx != EUI64_SIZE) {
    return false;
  }

  for (size_t i = 0; i < EUI64_SIZE; i++) {
    out_le[i] = bytes_be[EUI64_SIZE - 1 - i];
  }

  return is_valid_custom_eui64(out_le);
}

static bool get_configured_custom_eui64(sl_802154_long_addr_t out_eui64)
{
  sl_802154_long_addr_t restored_eui64;
  memset(restored_eui64, 0xFF, sizeof(restored_eui64));
  halCommonGetToken(&restored_eui64, TOKEN_STACK_RESTORED_EUI64);

  if (is_valid_custom_eui64(restored_eui64)) {
    memcpy(out_eui64, restored_eui64, EUI64_SIZE);
    return true;
  }

  sl_802154_long_addr_t mfg_eui64;
  memset(mfg_eui64, 0xFF, sizeof(mfg_eui64));
  halCommonGetMfgToken(&mfg_eui64, TOKEN_MFG_CUSTOM_EUI_64);

  if (is_valid_custom_eui64(mfg_eui64)) {
    memcpy(out_eui64, mfg_eui64, EUI64_SIZE);
    return true;
  }

  return false;
}

// Strong implementation called by the Zigbee stack during sl_zigbee_init()
// to override the hardware EUI64 with a custom EUI64 from NVM3 or USERDATA.
void sl_zigbee_get_restored_eui64(sl_802154_long_addr_t eui64)
{
  sl_802154_long_addr_t custom_eui64;
  if (get_configured_custom_eui64(custom_eui64)) {
    memcpy(eui64, custom_eui64, EUI64_SIZE);
  }
}

static void ensure_custom_eui64_and_router_state(void)
{
  tokTypeStackNodeData node_data;
  memset(&node_data, 0xFF, sizeof(node_data));
  halCommonGetToken(&node_data, TOKEN_STACK_NODE_DATA);

  // If NVRAM still holds coordinator/non-router network state (for example when
  // repurposing a former coordinator as a router), reset TOKEN_STACK_NODE_DATA
  // while preserving any custom EUI64 stored in NVM3/USERDATA.
  if (node_data.panId != 0xFFFF
      && node_data.nodeType != SL_ZIGBEE_ROUTER
      && node_data.nodeType != SL_ZIGBEE_UNKNOWN_DEVICE) {
    sl_802154_long_addr_t saved_eui64;
    bool has_custom = get_configured_custom_eui64(saved_eui64);

    tokTypeStackNodeData default_node_data = { 0xFFFF, -1, 0, 0x00, 0x00, 0x0000 };
    halCommonSetToken(TOKEN_STACK_NODE_DATA, &default_node_data);

    if (has_custom) {
      halCommonSetToken(TOKEN_STACK_RESTORED_EUI64, &saved_eui64);
    }

    halReboot();
  }

  // Ensure a custom EUI64 from NVM3 (TOKEN_STACK_RESTORED_EUI64) or USERDATA
  // (TOKEN_MFG_CUSTOM_EUI_64) is active as the local node EUI64.
  sl_802154_long_addr_t custom_eui64;
  if (get_configured_custom_eui64(custom_eui64)) {
    sl_802154_long_addr_t restored_eui64;
    memset(restored_eui64, 0xFF, sizeof(restored_eui64));
    halCommonGetToken(&restored_eui64, TOKEN_STACK_RESTORED_EUI64);

    // Mirror USERDATA custom EUI64 into TOKEN_STACK_RESTORED_EUI64 if not set
    // yet so sl_zigbee_get_restored_eui64() always supplies it at stack init.
    if (!is_valid_custom_eui64(restored_eui64)) {
      halCommonSetToken(TOKEN_STACK_RESTORED_EUI64, &custom_eui64);
    }

    if (memcmp(sl_zigbee_get_eui64(), custom_eui64, EUI64_SIZE) != 0) {
      memcpy(sl_zigbee_get_eui64(), custom_eui64, EUI64_SIZE);
      halReboot();
    }
  }
}

//---------------
// Custom CLI Commands

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint16_t patch;
} gecko_version_t;

gecko_version_t parse_version(uint32_t version){
   gecko_version_t result;

   result.major = (version >> 24) & 0xFF;
   result.minor = (version >> 16) & 0xFF;
   result.patch = version & 0xFFFF;

   return result;
}

void bootloader_info(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  char version_str[16];
  BootloaderInformation_t info = { .type = SL_BOOTLOADER, .version = 0L, .capabilities = 0L };

  bootloader_getInfo(&info);
  printf("%s Bootloader", info.type == SL_BOOTLOADER ? "Gecko" : "Legacy" );

  gecko_version_t ver = parse_version(info.version);
  snprintf(version_str, sizeof(version_str),
          "v%u.%02u.%02u", ver.major, ver.minor, ver.patch);
  printf(" %s\n", version_str);

}

void bootloader_reboot(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  bootloader_rebootAndInstall();
}

void eui64_info_cmd(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  sl_802154_long_addr_t restored_eui64;
  sl_802154_long_addr_t mfg_eui64;
  sl_802154_long_addr_t hw_eui64;

  memset(restored_eui64, 0xFF, sizeof(restored_eui64));
  memset(mfg_eui64, 0xFF, sizeof(mfg_eui64));
  memset(hw_eui64, 0xFF, sizeof(hw_eui64));

  halCommonGetToken(&restored_eui64, TOKEN_STACK_RESTORED_EUI64);
  halCommonGetMfgToken(&mfg_eui64, TOKEN_MFG_CUSTOM_EUI_64);
  halCommonGetMfgToken(&hw_eui64, TOKEN_MFG_EMBER_EUI_64);

  print_eui64_be("Active EUI64", sl_zigbee_get_eui64());
  print_eui64_be("NVM3 Restored EUI64", restored_eui64);
  print_eui64_be("USERDATA Custom EUI64", mfg_eui64);
  print_eui64_be("Hardware EUI64", hw_eui64);
}

void eui64_set_cmd(sl_cli_command_arg_t *arguments)
{
  const char *eui_str = sl_cli_get_argument_string(arguments, 0);
  sl_802154_long_addr_t new_eui64;

  if (!parse_eui64_be_str(eui_str, new_eui64)) {
    printf("Invalid EUI64. Expected 8 hex bytes, e.g. 00:11:22:33:44:55:66:77\n");
    return;
  }

  halCommonSetToken(TOKEN_STACK_RESTORED_EUI64, &new_eui64);
  print_eui64_be("Saved custom EUI64 to NVM3, rebooting", new_eui64);
  halReboot();
}

void eui64_reset_cmd(sl_cli_command_arg_t *arguments)
{
  (void)arguments;
  sl_802154_long_addr_t blank_eui64;
  memset(blank_eui64, 0xFF, sizeof(blank_eui64));

  halCommonSetToken(TOKEN_STACK_RESTORED_EUI64, &blank_eui64);
  printf("Cleared NVM3 custom EUI64, rebooting\n");
  halReboot();
}

static const sl_cli_command_info_t cmd__btl_info = \
  SL_CLI_COMMAND(bootloader_info,
                 "Bootloader info",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cmd__btl_reboot = \
  SL_CLI_COMMAND(bootloader_reboot,
                 "Enter bootloader",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_entry_t btl_group_table[] = {
    { "info", &cmd__btl_info, false },
    { "reboot", &cmd__btl_reboot, false },
    { NULL, NULL, false },
};

sl_cli_command_info_t cli_cmd_btl_group = \
  SL_CLI_COMMAND_GROUP(btl_group_table, "Bootloader commands");

static const sl_cli_command_info_t cmd__eui64_info = \
  SL_CLI_COMMAND(eui64_info_cmd,
                 "Print active, NVM3, USERDATA and hardware EUI64",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_info_t cmd__eui64_set = \
  SL_CLI_COMMAND(eui64_set_cmd,
                 "Set custom EUI64 in NVM3 and reboot",
                 "EUI64 in big-endian hex" SL_CLI_UNIT_SEPARATOR,
                 {SL_CLI_ARG_STRING, SL_CLI_ARG_END, });

static const sl_cli_command_info_t cmd__eui64_reset = \
  SL_CLI_COMMAND(eui64_reset_cmd,
                 "Clear custom EUI64 in NVM3 and reboot",
                 "none",
                 {SL_CLI_ARG_END, });

static const sl_cli_command_entry_t eui64_group_table[] = {
    { "info", &cmd__eui64_info, false },
    { "set", &cmd__eui64_set, false },
    { "reset", &cmd__eui64_reset, false },
    { NULL, NULL, false },
};

sl_cli_command_info_t cli_cmd_eui64_group = \
  SL_CLI_COMMAND_GROUP(eui64_group_table, "Custom EUI64 commands");

// Create root command table
const sl_cli_command_entry_t sl_cli_btl_command_table[] = {
  { "bootloader", &cli_cmd_btl_group, false },
  { "eui64", &cli_cmd_eui64_group, false },
  { NULL, NULL, false },
};

sl_cli_command_group_t sl_cli_btl_command_group =
{
  { NULL },
  false,
  sl_cli_btl_command_table
};

//---------------
// Custom helper functions
static void sync_on_off_cluster(uint8_t endpoint){
  bool onOff;
  if (sl_zigbee_af_read_server_attribute(endpoint,
                                         ZCL_ON_OFF_CLUSTER_ID,
                                         ZCL_ON_OFF_ATTRIBUTE_ID,
                                         (uint8_t *)&onOff,
                                         sizeof(onOff))
      == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    if (onOff) {
      led_turn_on(ON_OFF_LIGHT_LED);
    } else {
      led_turn_off(ON_OFF_LIGHT_LED);
    }
  }
}

//---------------
// Event handlers

static void commissioning_led_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
  if (sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK) {
    uint16_t identifyTime;
    sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                       ZCL_IDENTIFY_CLUSTER_ID,
                                       ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                       (uint8_t *)&identifyTime,
                                       sizeof(identifyTime));
    if (identifyTime > 0 && identifyTime < 60) {
      led_toggle(COMMISSIONING_STATUS_LED);
      sl_zigbee_af_event_set_delay_ms(&commissioning_led_event,
                                      LED_BLINK_PERIOD_MS << 1);
    } else {
      sync_on_off_cluster(LIGHT_ENDPOINT);
    }
  } else {
    sl_status_t status = sl_zigbee_af_network_steering_start();
    sl_zigbee_app_debug_println("%s network %s: 0x%X", "Join", "start", status);
  }
}

static void finding_and_binding_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
  if (sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK) {
    sl_zigbee_af_event_set_inactive(&finding_and_binding_event);

    sl_zigbee_app_debug_println("Find and bind target start: 0x%X",
                                sl_zigbee_af_find_and_bind_target_start(LIGHT_ENDPOINT));
  }
}

//----------------------
// Implemented Callbacks

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action. The framework
 * will always process the stack status after the callback returns.
 */
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  // Note, the ZLL state is automatically updated by the stack and the plugin.
  if (status == SL_STATUS_NETWORK_DOWN) {
    led_turn_off(COMMISSIONING_STATUS_LED);
    sl_zigbee_af_event_set_active(&commissioning_led_event);
  } else if (status == SL_STATUS_NETWORK_UP) {
    led_turn_on(COMMISSIONING_STATUS_LED);

    // Send announcement when coming online so coordinator detects without waiting for ping
    sl_zigbee_send_device_announcement();
    sl_zigbee_af_event_set_active(&finding_and_binding_event);
  }
}

/** @brief Init
 * Application init function
 */
void sl_zigbee_af_main_init_cb(void)
{
  ensure_custom_eui64_and_router_state();

  sl_zigbee_af_event_init(&commissioning_led_event, commissioning_led_event_handler);
  sl_zigbee_af_isr_event_init(&finding_and_binding_event, finding_and_binding_event_handler);

  sl_zigbee_af_event_set_active(&commissioning_led_event);

  // Custom bootloader and EUI64 CLI commands
  sl_cli_command_add_command_group(sl_cli_bootloader_handle, &sl_cli_btl_command_group);
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to SL_STATUS_OK to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt.
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID.
 *
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network.
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete.
 */
void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  (void)totalBeacons;
  (void)joinAttempts;
  (void)finalState;
  sl_zigbee_app_debug_println("Join network complete: 0x%X", status);

  if (status != SL_STATUS_OK) {
#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
    // Initialize our ZLL security now so that we are ready to be a touchlink
    // target at any point.
    status = sl_zigbee_af_zll_set_initial_security_state();
    if (status != SL_STATUS_OK) {
      sl_zigbee_app_debug_println("Error: cannot initialize ZLL security: 0x%X", status);
    }
#endif //SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT

    sl_zigbee_af_event_set_delay_ms(&commissioning_led_event,
                                    STEERING_DELAY_MS);
  }
}


/** @brief Post Attribute Change
 *
 * This function is called by the application framework after it changes an
 * attribute value. The value passed into this callback is the value to which
 * the attribute was set by the framework.
 */
void sl_zigbee_af_post_attribute_change_cb(uint8_t endpoint,
                                           sl_zigbee_af_cluster_id_t clusterId,
                                           sl_zigbee_af_attribute_id_t attributeId,
                                           uint8_t mask,
                                           uint16_t manufacturerCode,
                                           uint8_t type,
                                           uint8_t size,
                                           uint8_t* value)
{
  (void)manufacturerCode;
  (void)type;
  (void)size;
  (void)value;
  if (clusterId == ZCL_ON_OFF_CLUSTER_ID
      && attributeId == ZCL_ON_OFF_ATTRIBUTE_ID
      && mask == CLUSTER_MASK_SERVER) {
    sync_on_off_cluster(endpoint);
  }
}
/** @brief Identify Start Feedback
 *
 * @param endpoint The identifying endpoint Ver.: always
 * @param identifyTime The identify time Ver.: always
 */
void sl_zigbee_af_identify_start_feedback_cb(uint8_t endpoint,
                                             uint16_t identifyTime)
{
  (void)endpoint;
  sl_zigbee_app_debug_println("Identify start: endpoint=%d, time=%d",
                              endpoint, identifyTime);
  if (identifyTime > 0 && identifyTime < 60) {
    sl_zigbee_app_debug_println("Identify run: endpoint=%d, time=%d",
                              endpoint, identifyTime);
    sl_zigbee_af_event_set_delay_ms(&commissioning_led_event,
                                  LED_BLINK_PERIOD_MS);
  }
}

void sl_zigbee_af_identify_stop_feedback_cb(uint8_t endpoint)
{
  (void)endpoint;
  sl_zigbee_app_debug_println("Identify stop: endpoint=%d", endpoint);
}
/** @brief On/off Cluster Server Post Init
 *
 * Following resolution of the On/Off state at startup for this endpoint, perform any
 * additional initialization needed; e.g., synchronize hardware state.
 *
 * @param endpoint Endpoint that is being initialized
 */
void sl_zigbee_af_on_off_cluster_server_post_init_cb(uint8_t endpoint)
{
  // At startup, trigger a read of the attribute and possibly a toggle of the
  // LED to make sure they are always in sync.
  sl_zigbee_af_post_attribute_change_cb(endpoint,
                                        ZCL_ON_OFF_CLUSTER_ID,
                                        ZCL_ON_OFF_ATTRIBUTE_ID,
                                        CLUSTER_MASK_SERVER,
                                        0,
                                        0,
                                        0,
                                        NULL);
}

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  #ifndef EZSP_HOST
  sl_mac_calibrate_current_channel();
  #endif
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && (SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0)
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#include "force-sleep-wakeup.h"
#endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
/***************************************************************************//**
 * A callback called in interrupt context whenever a button changes its state.
 *
 * @remark Can be implemented by the application if required. This function
 * can contain the functionality to be executed in response to changes of state
 * in each of the buttons, or callbacks to appropriate functionality.
 *
 * @note The button state should not be updated in this function, it is updated
 * by specific button driver prior to arriving here
 *
   @param[out] handle             Pointer to button instance
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED) {
    sl_zigbee_af_event_set_active(&finding_and_binding_event);
    #ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
    sl_zigbee_app_framework_force_wakeup();
    #endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
  }
}
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT && SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0

#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
void sli_zigbee_app_framework_force_sleep_callback(void)
{
  // Do other things like turn off LEDs etc
  sl_led_turn_off(&sl_led_led0);
}
#endif // SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#endif //#if (LARGE_NETWORK_TESTING == 0)
