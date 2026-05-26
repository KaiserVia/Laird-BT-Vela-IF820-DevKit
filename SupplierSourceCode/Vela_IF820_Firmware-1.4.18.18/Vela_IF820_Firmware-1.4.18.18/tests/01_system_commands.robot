*** Settings ***
Documentation       System Command Tests.
...                 Will test system commands on an IF820 device.

Resource            common.robot

Suite Setup         Suite Setup
Suite Teardown      Suite Teardown
Test Timeout        5 seconds

Default Tags        vela if820


*** Variables ***
${REBOOT_COUNT}             3
${BOOT_DELAY_SECONDS}       ${0.25}


*** Test Cases ***
Ping
    Set Tags    PROD-675
    Send Command    ${lib_ez_serial_port.CMD_PING}

Query Firmware
    Set Tags    PROD-684
    Send Command    ${lib_ez_serial_port.CMD_QUERY_FW}

Get MAC Addr
    Set Tags    PROD-690
    Send Command    ${lib_ez_serial_port.CMD_GET_BT_ADDR}

Get Sleep Params
    Set Tags    PROD-688
    Send Command    ${lib_ez_serial_port.CMD_GET_SLEEP_PARAMS}

Get Tx Power
    Set Tags    PROD-687
    Send Command    ${lib_ez_serial_port.CMD_GET_TX_POWER}

Get Transport
    Set Tags    PROD-686
    Send Command    ${lib_ez_serial_port.CMD_GET_TRANSPORT}

Get Uart Params
    Set Tags    PROD-689
    FOR    ${flow}    IN    @{UART_FLOW_TYPES}
        Setup Uarts    ${flow}
        FOR    ${api_mode}    IN    @{API_MODES}
            EZ Send DUT1
            ...    ${lib_ez_serial_port.CMD_GET_UART_PARAMS}
            ...    ${api_mode}
            ...    uart_type=${0}
        END
        EZ Port Close    ${if820_board1}
    END

Factory Reset
    [Timeout]    40 seconds
    Set Tags    PROD-682
    FOR    ${flow}    IN    @{UART_FLOW_TYPES}
        Setup Uarts    ${flow}
        FOR    ${api_mode}    IN    @{API_MODES}
            EZ Send DUT1
            ...    ${lib_ez_serial_port.CMD_FACTORY_RESET}
            ...    ${api_mode}
            EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SYSTEM_BOOT}
            # Delay to allow flushing of boot events
            Sleep    ${BOOT_DELAY_SECONDS}
            # Re-apply UART params after factory reset
            Set Uart Params    ${flow}
            # Turn auto parse back on after reboot
            EZ Enable Protocol Auto Parse Mode    ${if820_board1}
        END
        EZ Port Close    ${if820_board1}
    END

Reboot
    Set Tags    PROD-678
    [Timeout]    30 seconds
    FOR    ${flow}    IN    @{UART_FLOW_TYPES}
        Setup Uarts    ${flow}
        FOR    ${api_mode}    IN    @{API_MODES}
            Reboot Command    ${api_mode}
        END
        EZ Port Close    ${if820_board1}
    END

Reboot Loop
    [Timeout]    1 minute
    Set Tags    PROD-699
    FOR    ${api_mode}    IN    @{API_MODES}
        FOR    ${flow}    IN    @{UART_FLOW_TYPES}
            Setup Uarts    ${flow}
            FOR    ${counter}    IN RANGE    ${REBOOT_COUNT}
                Reboot Command    ${api_mode}
            END
            EZ Port Close    ${if820_board1}
        END
    END


*** Keywords ***
Suite Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}

Suite Teardown
    De-Init Board    ${if820_board1}

Send Command
    [Arguments]    ${cmd}
    FOR    ${flow}    IN    @{UART_FLOW_TYPES}
        Setup Uarts    ${flow}
        FOR    ${api_mode}    IN    @{API_MODES}
            EZ Send DUT1    ${cmd}    ${api_mode}
        END
        EZ Port Close    ${if820_board1}
    END

Set Uart Params
    [Arguments]    ${flow}
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_SET_UART_PARAMS}
    ...    ${API_MODE_TEXT}
    ...    baud=${lib_ez_serial_port.IF820_DEFAULT_BAUD}
    ...    autobaud=${0}
    ...    autocorrect=${0}
    ...    flow=${flow}
    ...    databits=${8}
    ...    parity=${0}
    ...    stopbits=${1}
    ...    uart_type=${0}

Setup Uarts
    [Arguments]    ${flow}
    EZ Port Open    ${if820_board1}    ${flow}
    Set Uart Params    ${flow}

Reboot Command
    [Arguments]    ${api_mode}
    EZ Send DUT1
    ...    ${lib_ez_serial_port.CMD_REBOOT}
    ...    ${api_mode}
    EZ Wait Event DUT1    ${lib_ez_serial_port.EVENT_SYSTEM_BOOT}
    # Delay to allow flushing of boot events
    Sleep    ${BOOT_DELAY_SECONDS}
    # Turn auto parse back on after reboot
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}
