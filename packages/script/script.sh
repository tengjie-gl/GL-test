#!/bin/sh /etc/rc.common

START=99
STOP=10


CONFIG_FILE="/etc/config/wireless"
LOG_FILE="/tmp/my-wifi.log"

start() {
    uci set wireless.default_radio1.ssid='gl-inet-5G'
    uci set wireless.default_radio1.encryption='psk2'
    uci set wireless.default_radio1.key='goodlife'
    
    uci commit wireless

    wifi reload
}