*** Settings ***
Library     OperatingSystem
Library     ..${/}common_lib${/}libraries${/}If820Board.py
Library     ..${/}common_lib${/}libraries${/}BT900SerialPort.py
Library     ..${/}common_lib${/}libraries${/}EzSerialPort.py


*** Variables ***
${API_MODE_TEXT}                ${0}
${API_MODE_BINARY}              ${1}
${UART_FLOW_CNTRL_NONE}         ${0}
${UART_FLOW_CNTRL_ENABLE}       ${1}
${DEFAULT_RX_TIMEOUT}           ${1}
${CLEAR_QUEUE_TIMEOUT_SEC}      ${2}
${ERROR_DEVICE_TYPE}            Error! Device type not found!
${BOOT_DELAY_SECONDS}           ${3}
@{API_MODES}                    ${API_MODE_TEXT}    ${API_MODE_BINARY}
@{UART_FLOW_TYPES}              ${UART_FLOW_CNTRL_NONE}    ${UART_FLOW_CNTRL_ENABLE}
${OK}                           OK
${OTA_LATENCY_SECONDS}          ${0.1}


*** Keywords ***
Fail on error
    [Arguments]    ${err}
    IF    ${err} != 0    Fail    Error! Response error: ${err}

Fail if not equal
    [Arguments]    ${expected_value}    ${my_value}
    IF    ${expected_value} != ${my_value}
        Fail    Error! Expected Value: ${expected_value} is NOT EQUAL to: ${my_value}
    END

Find Boards and Settings
    # Delay in case boards are re-enumerating over USB
    Sleep    ${BOOT_DELAY_SECONDS}

    ${lib_ez_serial_port}=    Get Library Instance    EzSerialPort
    Set Global Variable    ${lib_ez_serial_port}    ${lib_ez_serial_port}

    ${bt900_board1}=    Get Library Instance    BT900SerialPort
    Set Global Variable    ${bt900_board1}    ${bt900_board1}

    ${settings_file}=    Get File    ${CURDIR}${/}..${/}.vscode${/}settings.json
    ${settings}=    Evaluate    json.loads('''${settings_file}''')    json

    @{if820_boards}=    If820Board.Get Connected Boards
    ${num_boards}=    Get Length    ${if820_boards}
    Log    ${num_boards} IF820 Boards Found!

    Set Global Variable    ${settings_bt900_board1_comport}    ${settings["comport_BT900_device1"]}

    IF    ${num_boards} == ${0}
        Fail    Error! No IF820 boards found!
    ELSE
        Set Global Variable    ${if820_board1}    ${if820_boards[0]}
    END
    IF    ${num_boards} > ${1}
        Set Global Variable    ${if820_board2}    ${if820_boards[1]}
    END

    RETURN    ${settings}

Init Board
    [Arguments]    ${board}    ${wait_for_boot}=${True}

    ${board_ready}=    Set Variable    ${board.is_initialized}
    IF    ${board_ready} == False
        Call Method    ${board}    open_and_init_board    ${wait_for_boot}
    END

De-Init Board
    [Arguments]    ${board}    ${reset_probe}=${True}

    ${board_ready}=    Set Variable    ${board.is_initialized}
    IF    ${board_ready} == True
        Call Method    ${board}    close_ports_and_reset    ${reset_probe}
    END

EZ Set API Mode
    [Arguments]    ${board}    ${apiformat}

    Call Method    ${board.p_uart}    set_api_format    ${apiformat}

EZ Send
    [Arguments]    ${board}    ${command}    ${apiformat}=${None}    &{kwargs}

    ${res}=    Call Method
    ...    ${board.p_uart}
    ...    send_and_wait
    ...    ${command}
    ...    ${apiformat}
    ...    &{kwargs}
    Fail on error    ${res[0]}
    RETURN    ${res[1]}

EZ Wait Event
    [Arguments]    ${board}    ${event}    ${timeout}=${DEFAULT_RX_TIMEOUT}

    ${res}=    Call Method    ${board.p_uart}    wait_event    ${event}    ${timeout}
    Fail on error    ${res[0]}
    RETURN    ${res[1]}

EZ Send DUT1
    [Arguments]    ${command}    ${apiformat}=${None}    &{kwargs}

    ${res}=    EZ Send    ${if820_board1}    ${command}    ${apiformat}    &{kwargs}
    RETURN    ${res}

EZ Wait Event DUT1
    [Arguments]    ${event}    ${timeout}=${DEFAULT_RX_TIMEOUT}

    ${res}=    EZ Wait Event    ${if820_board1}    ${event}    ${timeout}
    RETURN    ${res}

EZ Send DUT2
    [Arguments]    ${command}    ${apiformat}=${None}    &{kwargs}

    ${res}=    EZ Send    ${if820_board2}    ${command}    ${apiformat}    &{kwargs}
    RETURN    ${res}

EZ Wait Event DUT2
    [Arguments]    ${event}    ${timeout}=${DEFAULT_RX_TIMEOUT}

    ${res}=    EZ Wait Event    ${if820_board2}    ${event}    ${timeout}
    RETURN    ${res}

EZ Port Open
    [Arguments]    ${board}    ${flow_control}=${False}

    ${res}=    Call Method
    ...    ${board.p_uart}
    ...    open
    ...    ${board.puart_port_name}
    ...    ${board.p_uart.IF820_DEFAULT_BAUD}
    ...    ${flow_control}

EZ Port Close
    [Arguments]    ${board}

    Call Method    ${board.p_uart}    close

EZ Enable Protocol Auto Parse Mode
    [Documentation]    Enable protocol auto parse mode on the specified board. (v1.4.18.18 firmware and later)
    [Arguments]    ${board}

    EZ Send    ${board}    ${lib_ez_serial_port.CMD_PROTOCOL_SET_PARSE_MODE}    mode=${128}

UTF8 Bytes to String
    [Arguments]    ${utf8_bytes}
    ${byte_string}=    Convert To Bytes    ${utf8_bytes}
    ${utf8_string}=    Convert To String    ${byte_string}
    RETURN    ${utf8_string}

IF820 Flash Firmware
    [Arguments]    ${board}    ${mini_driver}    ${firmware_file}    ${chip_erase}=${False}

    De-Init Board    ${board}    ${False}
    ${res}=    Call Method    ${board}    flash_firmware    minidriver=${mini_driver}    firmware=${firmware_file}    chip_erase=${chip_erase}    verify=True

IF820 Query Firmware Version
    [Arguments]    ${board}

    ${res}=    EZ Send    ${board}    ${lib_ez_serial_port.CMD_QUERY_FW}
    ${app_ver}=    Convert To Hex    ${res.payload.app}    prefix=0x    length=8
    RETURN    ${app_ver}

IF820 Query Bluetooth Address
    [Arguments]    ${board}

    ${res}=    EZ Send    ${board}    ${lib_ez_serial_port.CMD_GET_BT_ADDR}
    RETURN    ${res.payload.address}

IF820 Query Bluetooth Address String
    [Arguments]    ${board}

    ${res}=    IF820 Query Bluetooth Address    ${board}
    ${bt_addr}=    If820Board.if820_mac_addr_response_to_mac_as_string    ${res}
    RETURN    ${bt_addr}

Get DVK Probe Firmware Version
    [Arguments]    ${board}

    ${res}=    Call Method    ${board.probe}    get_dap_info    ${9}
    RETURN    ${res}

DVK Probe Set IO Dir
    [Documentation]    Set the IO direction of a DVK probe GPIO
    [Arguments]    ${board}    ${gpio}    ${io_dir}    ${io_cfg}=${0}

    IF    ${io_dir}
        ${res}=    Call Method    ${board.probe}    gpio_to_output    ${gpio}    ${io_cfg}
    ELSE
        ${res}=    Call Method    ${board.probe}    gpio_to_input    ${gpio}    ${io_cfg}
    END

DVK Probe Set IO
    [Documentation]    Set the IO value of a DVK probe GPIO
    [Arguments]    ${board}    ${gpio}    ${io_val}

    IF    ${io_val}
        ${res}=    Call Method    ${board.probe}    gpio_to_output_high    ${gpio}
    ELSE
        ${res}=    Call Method    ${board.probe}    gpio_to_output_low    ${gpio}
    END

DVK Probe Read IO
    [Documentation]    Read the IO value of a DVK probe GPIO
    [Arguments]    ${board}    ${gpio}

    ${res}=    Call Method    ${board.probe}    gpio_read    ${gpio}
    RETURN    ${res}

EZ Send Raw
    [Arguments]    ${board}    ${data}

    ${res}=    Call Method    ${board.p_uart}    send    ${data}

EZ Read Raw
    [Arguments]    ${board}

    ${res}=    Call Method    ${board.p_uart}    read
    RETURN    ${res}

EZ Clear RX Buffer
    [Arguments]    ${board}

    ${res}=    Call Method    ${board.p_uart}    clear_rx_queue

Init BT900
    Call Method    ${bt900_board1}    open    ${settings_bt900_board1_comport}
    ${res}=    BT900 Send    ${bt900_board1.BT900_CMD_QUERY_FW}    ${False}

BT900 Send
    [Arguments]    ${command}    ${confirm_resp}=${True}

    ${res}=    Call Method    ${bt900_board1}    send    ${command}
    IF    ${confirm_resp}    Should Contain    ${res}    ${OK}
    RETURN    ${res}

De-Init BT900
    Call Method    ${bt900_board1}    close

BT900 Enter Command Mode
    ${res}=    Call Method    ${bt900_board1}    enter_command_mode
    Should Contain    ${res}    ${OK}

BT900 Exit Command Mode
    Call Method    ${bt900_board1}    exit_command_mode

BT900 Clear RX Buffer
    ${res}=    Call Method    ${bt900_board1}    clear_rx_queue

BT900 Read Raw
    ${res}=    Call Method    ${bt900_board1}    read
    RETURN    ${res}

BT900 Get MAC Address
    [Documentation]    Note: This function cannot be called when the BT900 is in command mode.
    ${res}=    Call Method    ${bt900_board1}    get_bt900_bluetooth_mac
    RETURN    ${res}

BT900 Wait Response
    [Arguments]    ${timeout}=${DEFAULT_RX_TIMEOUT}

    ${res}=    Call Method    ${bt900_board1}    wait_for_response    ${timeout}
    RETURN    ${res}

IF820 SPP Send and Receive data
    [Arguments]    ${sender}    ${receiver}    ${data}    ${latency}=${OTA_LATENCY_SECONDS}

    # clear any RX data on the receiver side to prepare for the TX data
    EZ Clear RX Buffer    ${receiver}
    # send the data
    EZ Send Raw    ${sender}    ${data}
    # wait for the data to be received over the air
    Sleep    ${latency}
    # read the received data and verify it
    ${rx_data}=    EZ Read Raw    ${receiver}
    ${rx_string}=    UTF8 Bytes to String    ${rx_data}
    Should Be Equal As Strings    ${rx_string}    ${data}

EZ Factory Reset
    [Arguments]    ${board}

    EZ Send    ${board}    ${lib_ez_serial_port.CMD_FACTORY_RESET}
    EZ Wait Event    ${board}    ${lib_ez_serial_port.EVENT_SYSTEM_BOOT}
    Sleep    ${0.25}    # Delay to allow flushing of boot events

BT900 IF820 SPP Send and Receive Data
    [Documentation]    Send and receive data over SPP between the BT900 and IF820.
    ...    If the direction is 0, the BT900 will receive the data and the IF820 will send it.
    ...    If the direction is 1, the BT900 will send the data and the IF820 will receive it.
    [Arguments]    ${direction}    ${data}    ${latency}=${OTA_LATENCY_SECONDS}

    # clear any RX data on the receiver side and transmit the data
    IF    ${direction} == 0
        BT900 Clear RX Buffer
        EZ Send Raw    ${if820_board1}    ${data}
    ELSE
        EZ Clear RX Buffer    ${if820_board1}
        ${send_cmd}=    Catenate
        ...    SEPARATOR=
        ...    ${bt900_board1.BT900_SPP_WRITE_PREFIX}
        ...    ${data}
        BT900 Send    ${send_cmd}
    END
    # wait for the data to be received over the air
    Sleep    ${latency}
    # read the received data and verify it
    IF    ${direction} == 0
        ${rx_data}=    BT900 Wait Response
        Should Contain    ${rx_data}    Data:
    ELSE
        ${rx_data}=    EZ Read Raw    ${if820_board1}
        ${rx_string}=    UTF8 Bytes to String    ${rx_data}
        Should Contain    ${rx_string}    ${data}
    END
