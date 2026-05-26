*** Settings ***
Documentation       SPP tests with Vela IF820 devices.

Library             Collections
Resource            common.robot

Test Setup          Test Setup
Test Teardown       Test Teardown
Test Timeout        1 minute

Default Tags        vela if820


*** Variables ***
${SPP_DATA}     abcdefghijklmnop


*** Test Cases ***
SPP BT900->IF820 Binary Mode
    Set Tags    PROD-680

    SPP Test    ${API_MODE_BINARY}

SPP BT900->IF820 Text Mode
    Set Tags    PROD-698

    SPP Test    ${API_MODE_TEXT}


*** Keywords ***
Test Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    EZ Factory Reset    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
    Init BT900
    BT900 Enter Command Mode

Test Teardown
    BT900 Exit Command Mode
    De-Init BT900
    # IF820 exit SPP data mode
    DVK Probe Set IO Dir    ${if820_board1}    ${if820_board1.CYSPP}    ${1}
    DVK Probe Set IO    ${if820_board1}    ${if820_board1.CYSPP}    ${1}
    Sleep    ${0.5}
    EZ Factory Reset    ${if820_board1}
    De-Init Board    ${if820_board1}

SPP Test
    [Arguments]    ${api_format}

    EZ Set API Mode    ${if820_board1}    ${api_format}

    ${str_mac} =    IF820 Query Bluetooth Address String    ${if820_board1}

    # bt900 delete all previous bonds
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_BOND_DEL}

    # bt900 set io cap
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_IOCAP}

    # bt900 set pairable
    BT900 Send    ${bt900_board1.BT900_CMD_SET_BTC_PAIRABLE}

    # bt900 connect spp port
    ${connect_command} =    Set Variable    ${bt900_board1.BT900_SPP_CONNECT_REQ}${str_mac}
    BT900 Send    ${connect_command}

    # IF820 Event (Text Info contains "P")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_PAIRING_REQUESTED}

    # bt900 event (Text Info contains "Pair Req")
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_PAIR_REQ}

    # bt900 set pairable (Response is "OK")
    BT900 Send    ${bt900_board1.BT900_CMD_BTC_PAIR_RESPONSE}

    # bt900 event (Text Info contains "Pair Result")
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_PAIR_RESULT}

    # IF820 Event (Text Info contains "PR")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_PAIRING_RESULT}

    # IF820 Event (Text Info contains "ENC")
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SMP_ENCRYPTION_STATUS}

    # bt900 event (Text Info contains "Pair Result")
    # Note: For some reason the BT900 sends the Pair Result event twice.
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_PAIR_RESULT}

    # IF820 connection Event
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_BT_CONNECTED}

    # bt900 connection event
    ${bt900_event} =    BT900 Wait Response
    Should Contain    ${bt900_event}    ${bt900_board1.BT900_SPP_CONNECT}

    # The two devices are connected.We can now send data on SPP.
    # send data from IF820 -> BT900
    BT900 IF820 SPP Send and Receive Data    0    ${SPP_DATA}    ${1}

    # send data from BT900 -> IF820
    BT900 IF820 SPP Send and Receive Data    1    ${SPP_DATA}    ${1}

    # disconnect
    BT900 Send    ${bt900_board1.BT900_SPP_DISCONNECT}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_BT_DISCONNECTED}
