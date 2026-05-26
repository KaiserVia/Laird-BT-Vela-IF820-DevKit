*** Settings ***
Documentation       BLE Test Suite

Library             String
Library             Collections
Resource            common.robot

Test Setup          Test Setup
Test Teardown       Test Teardown
Test Timeout        2 minutes

Default Tags        vela if820


*** Variables ***
${ADV_MODE}                                 ${2}
${ADV_TYPE}                                 ${3}
${ADV_INTERVAL}                             ${0x40}
${ADV_CHANNELS}                             ${0x07}
${ADV_FLAGS_CUSTOM_DATA}                    ${0x02}
${ADV_FLAGS_GATT}                           ${0x00}
# Flags, short name my_sensor, VS data uint16 LSB
@{ADV_DATA}
...                                         ${0x02}
...                                         ${0x01}
...                                         ${0x06}
...                                         ${0x0a}
...                                         ${0x08}
...                                         ${0x6d}
...                                         ${0x79}
...                                         ${0x5f}
...                                         ${0x73}
...                                         ${0x65}
...                                         ${0x6e}
...                                         ${0x73}
...                                         ${0x6f}
...                                         ${0x72}
...                                         ${0x05}
...                                         ${0xff}
...                                         ${0x77}
...                                         ${0x00}
...                                         ${0x01}
...                                         ${0x00}
${SCAN_MODE}                                ${2}
${SCAN_FILTER_ACCEPT_ALL}                   ${0}
@{DIRECT_ADDR}                              ${0}    ${0}    ${0}    ${0}    ${0}    ${0}

${GATT_ATTR_TYPE_STRUCTURE}                 ${0}
${GATT_ATTR_TYPE_VALUE}                     ${1}
${GATT_ATTR_PERMISSION_READ}                ${0x02}
${GATT_ATTR_PERMISSION_READ_AUTH_WRITE}     ${0x42}
${GATT_ATTR_PERMISSION_READ_WRITE_ACK}      ${0x0A}
${BATTERY_LEVEL}                            ${95}
${battery_value_handle}                     ${EMPTY}


*** Test Cases ***
BLE Custom Advertiser
    Set Tags    PROD-683

    Stop advertising
    Set advertising params    ${ADV_MODE}    ${ADV_FLAGS_CUSTOM_DATA}
    Set advertising data    ${ADV_DATA}
    Start advertising    ${ADV_MODE}    ${ADV_FLAGS_CUSTOM_DATA}
    Scan for custom Peripheral

BLE Custom GATT
    Set Tags    PROD-681

    Stop advertising
    Setup Battery Service
    Start advertising    ${ADV_MODE}    ${ADV_FLAGS_GATT}
    Connect to Peripheral
    Read Battery Voltage


*** Keywords ***
Test Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    Init Board    ${if820_board2}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board2}

    ${PERIPHERAL_ADDRESS}=    IF820 Query Bluetooth Address    ${if820_board2}
    Set Global Variable    ${PERIPHERAL_ADDRESS}    ${PERIPHERAL_ADDRESS}

Test Teardown
    De-Init Board    ${if820_board1}
    De-Init Board    ${if820_board2}

Set advertising params
    [Arguments]    ${mode}    ${flags}

    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GAP_SET_ADV_PARAMETERS}
    ...    mode=${mode}
    ...    type=${ADV_TYPE}
    ...    interval=${ADV_INTERVAL}
    ...    channels=${ADV_CHANNELS}
    ...    high_interval=${ADV_INTERVAL}
    ...    high_duration=${0}
    ...    low_interval=${ADV_INTERVAL}
    ...    low_duration=${0}
    ...    flags=${flags}
    ...    directAddr=${DIRECT_ADDR}
    ...    directAddrType=${0}

Set advertising data
    [Arguments]    ${data}

    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GAP_SET_ADV_DATA}
    ...    data=@{data}

Start advertising
    [Arguments]    ${mode}    ${flags}

    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GAP_START_ADV}
    ...    mode=${mode}
    ...    type=${ADV_TYPE}
    ...    channels=${ADV_CHANNELS}
    ...    high_interval=${ADV_INTERVAL}
    ...    high_duration=${0}
    ...    low_interval=${ADV_INTERVAL}
    ...    low_duration=${0}
    ...    flags=${flags}
    ...    directAddr=${DIRECT_ADDR}
    ...    directAddrType=${0}

Stop advertising
    EZ Send DUT2    ${lib_ez_serial_port.CMD_GAP_STOP_ADV}
    EZ Wait Event DUT2    ${lib_ez_serial_port.EVENT_GAP_ADV_STATE_CHANGED}

Scan for custom Peripheral
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_GAP_START_SCAN}
    ...    mode=${SCAN_MODE}
    ...    interval=${0x100}
    ...    window=${0x100}
    ...    active=${0}
    ...    filter=${SCAN_FILTER_ACCEPT_ALL}
    ...    nodupe=${1}
    ...    timeout=${120}
    WHILE    True
        ${res}=    EZ Wait Event DUT1
        ...    ${lib_ez_serial_port.EVENT_GAP_SCAN_RESULT}
        ${packet}=    Set Variable    ${res}
        IF    ${PERIPHERAL_ADDRESS} == ${packet.payload.address}
            Log    Discovered peripheral!
            ${received_data}=    Set Variable    ${packet.payload.data}
            FOR    ${index}    ${e}    IN ENUMERATE    @{ADV_DATA}
                IF    ${e} != ${received_data}[${index}]
                    Fail    Advertising data does not match!
                END
            END
            BREAK
        END
    END

Setup Battery Service
    # Create battery service descriptor
    ${data}=    Evaluate    bytearray.fromhex('00280F18')
    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GATTS_CREATE_ATTR}
    ...    type=${GATT_ATTR_TYPE_STRUCTURE}
    ...    perm=${GATT_ATTR_PERMISSION_READ}
    ...    length=${4}
    ...    data=${data}
    # Create battery level characteristic descriptor
    ${data}=    Evaluate    bytearray.fromhex('0328121800192A')
    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GATTS_CREATE_ATTR}
    ...    type=${GATT_ATTR_TYPE_STRUCTURE}
    ...    perm=${GATT_ATTR_PERMISSION_READ}
    ...    length=${7}
    ...    data=${data}
    # Create battery level characteristic value descriptor and set value
    ${data}=    Evaluate    bytearray([${BATTERY_LEVEL}])
    ${res}=    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GATTS_CREATE_ATTR}
    ...    type=${GATT_ATTR_TYPE_VALUE}
    ...    perm=${GATT_ATTR_PERMISSION_READ_AUTH_WRITE}
    ...    length=${1}
    ...    data=${data}
    Set Global Variable    ${battery_value_handle}    ${res.payload.handle}
    # Create battery level Client Characteristic Configuration descriptor
    ${data}=    Evaluate    bytearray.fromhex('02290000')
    EZ Send DUT2
    ...    ${lib_ez_serial_port.CMD_GATTS_CREATE_ATTR}
    ...    type=${GATT_ATTR_TYPE_STRUCTURE}
    ...    perm=${GATT_ATTR_PERMISSION_READ_WRITE_ACK}
    ...    length=${4}
    ...    data=${data}

Connect to Peripheral
    [Timeout]    20 seconds
    ${packet}=    Set Variable    ${EMPTY}
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_GAP_START_SCAN}
    ...    mode=${SCAN_MODE}
    ...    interval=${0x100}
    ...    window=${0x100}
    ...    active=${0}
    ...    filter=${SCAN_FILTER_ACCEPT_ALL}
    ...    nodupe=${1}
    ...    timeout=${120}
    WHILE    True
        ${res}=    EZ Wait Event DUT1
        ...    ${lib_ez_serial_port.EVENT_GAP_SCAN_RESULT}
        ${packet}=    Set Variable    ${res}
        IF    ${PERIPHERAL_ADDRESS} == ${packet.payload.address}
            Log    Discovered peripheral!
            BREAK
        END
    END
    EZ Send DUT1    ${lib_ez_serial_port.CMD_GAP_STOP_SCAN}    ${API_MODE_BINARY}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GAP_SCAN_STATE_CHANGED}
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_GAP_CONNECT}
    ...    address=${PERIPHERAL_ADDRESS}
    ...    type=${packet.payload.address_type}
    ...    interval=${6}
    ...    slave_latency=${0}
    ...    supervision_timeout=${100}
    ...    scan_interval=${0x0100}
    ...    scan_window=${0x0100}
    ...    scan_timeout=${0}

    # Wait for connection to be established
    EZ Wait Event DUT1
    ...    ${lib_ez_serial_port.EVENT_GAP_CONNECTED}

Read Battery Voltage
    [Timeout]    20 seconds

    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_GATTC_READ_HANDLE}
    ...    conn_handle=${0}
    ...    attr_handle=${battery_value_handle}
    ${res}=    EZ Wait Event DUT1
    ...    ${lib_ez_serial_port.EVENT_GATTC_DATA_RECEIVED}
    IF    ${res.payload.attr_handle} == ${battery_value_handle}
        IF    ${res.payload.data[0]} != ${BATTERY_LEVEL}
            Fail    Battery level is not ${BATTERY_LEVEL}
        END
    ELSE
        Fail    Unexpected attribute handle
    END
