*** Settings ***
Documentation       CYSPP tests with BT900 central to Vela IF820 peripheral.

Library             Collections
Resource            common.robot

Test Setup          Test Setup
Test Teardown       Test Teardown
Test Timeout        30 seconds

Default Tags        vela if820


*** Variables ***
${CYSPP_DATA}       abcdefghijklmnop


*** Test Cases ***
CYSPP BT900->IF820 Binary Mode
    Set Tags    PROD-697
    CYSPP Test    ${API_MODE_BINARY}

CYSPP BT900->IF820 Text Mode
    Set Tags    PROD-696
    CYSPP Test    ${API_MODE_TEXT}


*** Keywords ***
Test Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    EZ Factory Reset    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
    Init BT900

Test Teardown
    Disconnect BT900
    De-Init BT900
    EZ Factory Reset    ${if820_board1}
    De-Init Board    ${if820_board1}

Disconnect BT900
    BT900 Send    ${bt900_board1.BT900_GATTC_CLOSE}
    BT900 Send    ${bt900_board1.BT900_DISCONNECT}
    BT900 Exit Command Mode

CYSPP Test
    [Arguments]    ${api_format}

    EZ Set API Mode    ${if820_board1}    ${api_format}

    EZ Send DUT1    ${lib_ez_serial_port.CMD_GAP_STOP_ADV}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GAP_ADV_STATE_CHANGED}

    ${resp} =    IF820 Query Bluetooth Address String    ${if820_board1}
    ${str_mac} =    Set Variable    01${resp}

    BT900 Enter Command Mode

    # if820 advertise
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_GAP_START_ADV}
    ...    mode=${2}
    ...    type=${3}
    ...    channels=${7}
    ...    high_interval=${48}
    ...    high_duration=${30}
    ...    low_interval=${2048}
    ...    low_duration=${60}
    ...    flags=${0}
    ...    directAddr=${0}
    ...    directAddrType=${0}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GAP_ADV_STATE_CHANGED}

    # bt900 cyspp connect
    # note:    the bt900 central could scan for devices and pick out the appropriate device to connect to.
    # however for simplicity since we already know its address we will skip that step and just connect.
    ${connect_command} =    Set Variable
    ...    ${bt900_board1.BT900_CYSPP_CONNECT}${str_mac} 50 30 30 50
    ${response} =    BT900 Send    ${connect_command}

    # IF820 Event (Text Info contains "C" for connect)
    ${response} =    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GAP_CONNECTED}

    # IF820 Event (Text Info contains "CU" for connection updated)
    ${response} =    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GAP_CONNECTION_UPDATED}

    # bt900 open gattc
    ${response} =    BT900 Send    ${bt900_board1.BT900_GATTC_OPEN}

    # bt900 enable notifications
    ${response} =    BT900 Send    ${bt900_board1.BT900_ENABLE_CYSPP_NOT}

    # IF820 Event (Text Info contains "W" for gatts data written)
    ${response} =    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_GATTS_DATA_WRITTEN}

    # send data from IF820 -> BT900
    BT900 Clear RX Buffer
    EZ Send Raw    ${if820_board1}    ${CYSPP_DATA}
    # read data from BT900
    Sleep    ${OTA_LATENCY_SECONDS}
    ${rx_data} =    BT900 Read Raw
    ${utf8_string_bt900} =    UTF8 Bytes to String    ${rx_data}

    # send data from BT900 -> IF820
    EZ Clear RX Buffer    ${if820_board1}
    ${data_to_send} =    Catenate
    ...    SEPARATOR=
    ...    ${bt900_board1.BT900_CYSPP_WRITE_DATA_STRING}
    ...    ${CYSPP_DATA}
    ${response} =    BT900 Send    ${data_to_send}
    Sleep    ${OTA_LATENCY_SECONDS}
    # read data from IF820
    ${rx_data} =    EZ Read Raw    ${if820_board1}
    ${utf8_string_if820} =    UTF8 Bytes to String    ${rx_data}

    Builtin.Should Not Be Empty    ${utf8_string_bt900}
    Builtin.Should Not Be Empty    ${utf8_string_if820}
