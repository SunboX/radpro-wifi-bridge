/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Network/WaveshareEthernet.h"

#if defined(RADPRO_WAVESHARE_ESP32_S3_ETH)

#include <WiFi.h>
#include <esp_eth.h>
#include <esp_eth_mac.h>
#include <esp_eth_phy.h>
#include <esp_eth_netif_glue.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>

extern bool tcpipInit();
extern void add_esp_interface_netif(esp_interface_t interface, esp_netif_t *esp_netif);

namespace
{
constexpr spi_host_device_t kSpiHost = SPI2_HOST;
constexpr int kSclkPin = 13;
constexpr int kMisoPin = 12;
constexpr int kMosiPin = 11;
constexpr int kCsPin = 14;
constexpr int kResetPin = 9;
constexpr int kInterruptPin = 10;
constexpr int kPhyAddress = 1;
constexpr int kSpiClockHz = 20 * 1000 * 1000;

esp_netif_t *s_netif = nullptr;
esp_eth_handle_t s_ethHandle = nullptr;
spi_device_handle_t s_spiHandle = nullptr;
volatile bool s_started = false;
volatile bool s_linkUp = false;
volatile bool s_hasIp = false;

void onEthernetEvent(void *, esp_event_base_t, int32_t eventId, void *)
{
    switch (eventId)
    {
    case ETHERNET_EVENT_START:
        s_started = true;
        break;
    case ETHERNET_EVENT_CONNECTED:
        s_linkUp = true;
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        s_linkUp = false;
        s_hasIp = false;
        break;
    case ETHERNET_EVENT_STOP:
        s_started = false;
        s_linkUp = false;
        s_hasIp = false;
        break;
    default:
        break;
    }
}

void onIpEvent(void *, esp_event_base_t, int32_t eventId, void *)
{
    if (eventId == IP_EVENT_ETH_GOT_IP)
        s_hasIp = true;
    else if (eventId == IP_EVENT_ETH_LOST_IP)
        s_hasIp = false;
}

IPAddress ipField(const esp_ip4_addr_t &address)
{
    return IPAddress(address.addr);
}
} // namespace

namespace WaveshareEthernet
{
bool begin(const char *hostname)
{
    if (s_started)
        return true;

    if (!tcpipInit())
        return false;

    esp_netif_config_t netifConfig = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netifConfig);
    if (!s_netif)
        return false;

    if (hostname && hostname[0])
        esp_netif_set_hostname(s_netif, hostname);

    const esp_err_t isrResult = gpio_install_isr_service(0);
    if (isrResult != ESP_OK && isrResult != ESP_ERR_INVALID_STATE)
        return false;

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = kMosiPin;
    busConfig.miso_io_num = kMisoPin;
    busConfig.sclk_io_num = kSclkPin;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.data4_io_num = -1;
    busConfig.data5_io_num = -1;
    busConfig.data6_io_num = -1;
    busConfig.data7_io_num = -1;
    busConfig.max_transfer_sz = 0;
    busConfig.flags = 0;
    busConfig.intr_flags = 0;

    if (spi_bus_initialize(kSpiHost, &busConfig, SPI_DMA_CH_AUTO) != ESP_OK)
        return false;

    spi_device_interface_config_t deviceConfig = {};
    deviceConfig.command_bits = 16;
    deviceConfig.address_bits = 8;
    deviceConfig.mode = 0;
    deviceConfig.clock_speed_hz = kSpiClockHz;
    deviceConfig.spics_io_num = kCsPin;
    deviceConfig.queue_size = 20;

    if (spi_bus_add_device(kSpiHost, &deviceConfig, &s_spiHandle) != ESP_OK)
        return false;

    eth_w5500_config_t w5500Config = ETH_W5500_DEFAULT_CONFIG(s_spiHandle);
    w5500Config.int_gpio_num = kInterruptPin;

    eth_mac_config_t macConfig = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phyConfig = ETH_PHY_DEFAULT_CONFIG();
    phyConfig.phy_addr = kPhyAddress;
    phyConfig.reset_gpio_num = kResetPin;

    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500Config, &macConfig);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phyConfig);
    if (!mac || !phy)
        return false;

    esp_eth_config_t ethConfig = ETH_DEFAULT_CONFIG(mac, phy);
    if (esp_eth_driver_install(&ethConfig, &s_ethHandle) != ESP_OK || !s_ethHandle)
        return false;

    uint8_t macAddress[6] = {};
    if (esp_read_mac(macAddress, ESP_MAC_ETH) != ESP_OK ||
        esp_eth_ioctl(s_ethHandle, ETH_CMD_S_MAC_ADDR, macAddress) != ESP_OK)
        return false;

    if (esp_netif_attach(s_netif, esp_eth_new_netif_glue(s_ethHandle)) != ESP_OK)
        return false;

    add_esp_interface_netif(ESP_IF_ETH, s_netif);

    if (esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &onEthernetEvent, nullptr) != ESP_OK)
        return false;
    if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &onIpEvent, nullptr) != ESP_OK)
        return false;

    if (esp_eth_start(s_ethHandle) != ESP_OK)
        return false;

    s_started = true;
    return true;
}

bool started()
{
    return s_started;
}

bool linkUp()
{
    return s_started && s_linkUp;
}

bool connected()
{
    if (!s_started || !s_linkUp || !s_hasIp || !s_netif)
        return false;

    esp_netif_ip_info_t info = {};
    return esp_netif_get_ip_info(s_netif, &info) == ESP_OK && info.ip.addr != 0;
}

IPAddress localIP()
{
    if (!s_netif)
        return IPAddress();
    esp_netif_ip_info_t info = {};
    if (esp_netif_get_ip_info(s_netif, &info) != ESP_OK)
        return IPAddress();
    return ipField(info.ip);
}

IPAddress gatewayIP()
{
    if (!s_netif)
        return IPAddress();
    esp_netif_ip_info_t info = {};
    if (esp_netif_get_ip_info(s_netif, &info) != ESP_OK)
        return IPAddress();
    return ipField(info.gw);
}

IPAddress subnetMask()
{
    if (!s_netif)
        return IPAddress();
    esp_netif_ip_info_t info = {};
    if (esp_netif_get_ip_info(s_netif, &info) != ESP_OK)
        return IPAddress();
    return ipField(info.netmask);
}

String macAddress()
{
    if (!s_ethHandle)
        return String();

    uint8_t mac[6] = {};
    if (esp_eth_ioctl(s_ethHandle, ETH_CMD_G_MAC_ADDR, mac) != ESP_OK)
        return String();

    char text[18];
    snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(text);
}
} // namespace WaveshareEthernet

#else

namespace WaveshareEthernet
{
bool begin(const char *) { return false; }
bool started() { return false; }
bool linkUp() { return false; }
bool connected() { return false; }
IPAddress localIP() { return IPAddress(); }
IPAddress gatewayIP() { return IPAddress(); }
IPAddress subnetMask() { return IPAddress(); }
String macAddress() { return String(); }
} // namespace WaveshareEthernet

#endif
