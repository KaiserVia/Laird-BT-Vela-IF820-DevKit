*** Settings ***
Documentation       SPP tests with Vela IF820 devices.

Resource            common.robot

Test Setup          Test Setup
Test Teardown       Test Teardown
Test Timeout        1 minute

Default Tags        vela if820


*** Variables ***
${SPP_DATA}                 abcdefghijklmnop
${SPP_LATENCY_SECONDS}      ${1}


*** Test Cases ***
SPP IF820->IF820 Binary Mode
    Set Tags    PROD-693
    SPP Test    ${API_MODE_BINARY}

SPP IF820->IF820 Text Mode
    Set Tags    PROD-694
    SPP Test    ${API_MODE_TEXT}


*** Keywords ***
Test Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    Init Board    ${if820_board2}
    EZ Factory Reset    ${if820_board1}
    EZ Factory Reset    ${if820_board2}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board2}
    DVK Probe Set IO Dir    ${if820_board1}    ${if820_board1.CYSPP}    ${0}
    DVK Probe Set IO Dir    ${if820_board2}    ${if820_board2.CYSPP}    ${0}

Test Teardown
    EZ Factory Reset    ${if820_board1}
    EZ Factory Reset    ${if820_board2}
    De-Init Board    ${if820_board1}
    De-Init Board    ${if820_board2}

Connect to Peripheral
    [Documentation]    Request Central to connect to Periperal via Bluetooth
    [Arguments]    ${peripheral_address}

    ${resp} =    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_CONNECT}
    ...    address=${peripheral_address}
    ...    type=${1}
    RETURN    ${resp}

SPP Test
    [Arguments]    ${api_format}

    EZ Set API Mode    ${if820_board1}    ${api_format}
    EZ Set API Mode    ${if820_board2}    ${api_format}

    ${peripheral_address} =    IF820 Query Bluetooth Address    ${if820_board2}

    # The CYSPP pin should be high when not connected and in command mode
    ${io_state_p} =    DVK Probe Read IO    ${if820_board2}    ${if820_board2.CYSPP}
    ${io_state_c} =    DVK Probe Read IO    ${if820_board1}    ${if820_board1.CYSPP}
    Builtin.Should Be Equal    ${1}    ${io_state_p}
    Builtin.Should Be Equal    ${1}    ${io_state_c}

    ${resp} =    Connect to Peripheral    ${peripheral_address}
    ${conn_handle} =    Builtin.Get Variable Value    ${resp[1].payload.conn_handle}

    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_BT_CONNECTED}

    # The CYSPP pin should be low when a SPP Connection is active and in data mode
    ${io_state_p} =    DVK Probe Read IO    ${if820_board2}    ${if820_board2.CYSPP}
    ${io_state_c} =    DVK Probe Read IO    ${if820_board1}    ${if820_board1.CYSPP}
    Builtin.Should Be Equal    ${0}    ${io_state_p}
    Builtin.Should Be Equal    ${0}    ${io_state_c}

    # send data from central to peripheral
    IF820 SPP Send and Receive data    ${if820_board1}    ${if820_board2}    ${SPP_DATA}    ${SPP_LATENCY_SECONDS}

    # send data from peripheral to central
    IF820 SPP Send and Receive data    ${if820_board2}    ${if820_board1}    ${SPP_DATA}    ${SPP_LATENCY_SECONDS}

    # exit data mode and enter command mode
    DVK Probe Set IO Dir    ${if820_board1}    ${if820_board1.CYSPP}    ${1}
    DVK Probe Set IO Dir    ${if820_board2}    ${if820_board1.CYSPP}    ${1}
    DVK Probe Set IO    ${if820_board1}    ${if820_board1.CYSPP}    ${1}
    DVK Probe Set IO    ${if820_board2}    ${if820_board1.CYSPP}    ${1}
    EZ Wait Event    ${if820_board1}    ${lib_ez_serial_port.EVENT_BT_DISCONNECTED}
    EZ Wait Event    ${if820_board2}    ${lib_ez_serial_port.EVENT_BT_DISCONNECTED}
