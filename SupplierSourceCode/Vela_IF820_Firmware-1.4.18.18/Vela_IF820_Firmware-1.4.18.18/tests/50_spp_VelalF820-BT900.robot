*** Settings ***
Documentation       SPP: IF820 Central to BT900 Peripheral

Resource            common.robot
Library             Collections

Test Setup          Test Setup
Test Teardown       Test Teardown
Test Timeout        1 minute

Default Tags        vela if820


*** Variables ***
${SPP_DATA}     abcdefghijklmnop


*** Test Cases ***
SPP IF820->BT900 Binary Mode
    Set Tags    PROD-691

    SPP Test    ${API_MODE_BINARY}

SPP IF820->BT900 Text Mode
    Set Tags    PROD-692

    SPP Test    ${API_MODE_TEXT}


*** Keywords ***
Test Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    DVK Probe Set IO Dir    ${if820_board1}    ${if820_board1.CYSPP}    ${0}
    EZ Factory Reset    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
    Init BT900

Test Teardown
    BT900 Exit Command Mode
    De-Init BT900
    EZ Factory Reset    ${if820_board1}
    De-Init Board    ${if820_board1}

SPP Test
    [Arguments]    ${api_format}

    EZ Set API Mode    ${if820_board1}    ${api_format}

    ${peripheral_address} =    BT900 Get MAC Address
    BT900 Enter Command Mode

    # bt900 delete all previous bonds
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_BOND_DEL}

    # bt900 set io cap
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_IOCAP}

    # bt900 set pairable
    BT900 Send    ${bt900_board1.BT900_CMD_SET_BTC_PAIRABLE}

    # bt900 set connectable
    BT900 Send
    ...    ${bt900_board1.BT900_CMD_SET_BTC_CONNECTABLE}

    # bt900 set discoverable
    BT900 Send
    ...    ${bt900_board1.BT900_CMD_SET_BTC_DISCOVERABLE}

    # bt900 open spp port
    BT900 Send    ${bt900_board1.BT900_CMD_SPP_OPEN}

    # IF820(central) connect to BT900 (peripheral)
    Reverse List    ${peripheral_address[1]}
    ${response} =    EZ Send DUT1    ${lib_ez_serial_port.CMD_CONNECT}
    ...    address=${peripheral_address[1]}
    ...    type=${1}
    ${conn_handle} =    Get Variable Value    ${response[1].payload.conn_handle}

    # IF820 Event (Text Info contains "P")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_PAIRING_REQUESTED}

    # bt900 event (Text Info contains "Pair Req")
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_PAIR_REQ}

    # bt900 set pairable (Response is "OK")
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_PAIR_RESPONSE}

    # IF820 Event (Text Info contains "PR")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_PAIRING_RESULT}

    # bt900 event (Text Info contains "Pair Result")
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_PAIR_RESULT}

    # IF820 Event (Text Info contains "ENC")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_ENCRYPTION_STATUS}

    # IF820 connection event
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_BT_CONNECTED}

    # bt900 connection event
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_SPP_CONNECT}

    # The two devices are connected. We can now send data over SPP.
    # send data from IF820 -> BT900
    BT900 IF820 SPP Send and Receive Data    0    ${SPP_DATA}    ${1}

    # send data from BT900 -> IF820
    BT900 IF820 SPP Send and Receive Data    1    ${SPP_DATA}    ${1}

    # disconnect
    BT900 Send    ${bt900_board1.BT900_SPP_DISCONNECT}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_BT_DISCONNECTED}
