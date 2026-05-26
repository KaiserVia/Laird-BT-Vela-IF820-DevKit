*** Settings ***
Documentation       GAP Command Tests.
...                 Will test GAP commands on an IF820 device.

Resource            common.robot

Suite Setup         Suite Setup
Suite Teardown      Suite Teardown
Test Timeout        10 seconds

Default Tags        vela if820


*** Test Cases ***
GAP Device Name
    Set Tags    PROD-53708

    ${device_name} =    Set Variable    Test device 1
    @{gap_name_types} =    Create List    ${0}    ${1}

    FOR    ${flow}    IN    @{UART_FLOW_TYPES}
        Setup Uarts    ${flow}
        FOR    ${api_mode}    IN    @{API_MODES}
            FOR    ${gap_name_type}    IN    @{gap_name_types}
                EZ Send DUT1
                ...    ${lib_ez_serial_port.CMD_GAP_SET_DEVICE_NAME}
                ...    ${api_mode}
                ...    type=${gap_name_type}
                ...    name=${device_name}

                ${resp} =    EZ Send DUT1
                ...    ${lib_ez_serial_port.CMD_GAP_GET_DEVICE_NAME}
                ...    ${api_mode}
                ...    type=${gap_name_type}

                Should Be Equal    ${resp['name']}    ${device_name}
            END
        END
        EZ Port Close    ${if820_board1}
    END


*** Keywords ***
Suite Setup
    Find Boards and Settings
    Init Board    ${if820_board1}
    EZ Enable Protocol Auto Parse Mode    ${if820_board1}

Suite Teardown
    De-Init Board    ${if820_board1}

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
