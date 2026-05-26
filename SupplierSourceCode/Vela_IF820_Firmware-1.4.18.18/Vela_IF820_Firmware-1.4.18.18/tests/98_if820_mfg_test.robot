*** Settings ***
Documentation       IF820 Manufacturing Test
...                 This test will continue to loop to test IF820 modules

Library             Dialogs
Library             Collections
Library             DateTime
Resource            common.robot

Test Setup          Test Setup
Test Teardown       Test Teardown

Default Tags        vela if820


*** Variables ***
${MINI_DRIVER}                  ${CURDIR}${/}..${/}files${/}v1.4.18.18_int-ant${/}minidriver-20820A1-uart-patchram.hex
${FIRMWARE}
...                             ${CURDIR}${/}..${/}files${/}v1.4.18.18_int-ant${/}20251116_ezserial_app_VELA-IF820-INT-ANT-EVK_141818_v1.4.18.18_download.hex
${PROGRAM_FIRMWARE_TIMEOUT}     30 seconds
${TEST_TIMEOUT_SHORT}           2 seconds
@{module_result}                ${EMPTY}
${RESULTE_FILE_NAME}            if820_mfg_results
${result_file}                  ${EMPTY}

# Pairs of GPIO pins for loopback testing. First pin in the pair is the output.
# TODO: Need different GPIO pairs for integrated antenna module and external antenna module
@{GPIO_SETS}                    ${EMPTY}

@{GPIO_TEST_STATES}             ${1}    ${0}
${GPIO_CONFIG_OUTPUT}           ${0x4000}
${GPIO_CONFIG_INPUT}            ${0x0000}
${GPIO_OP_IMMEDIATE}            ${0}
${GPIO_OP_RELEASE}              ${4}


*** Test Cases ***
MFG Test
    Set Tags    PROD-695
    Manufacturing Test Loop


*** Keywords ***
Test Setup
    Find Boards and Settings

    Init Board    ${if820_board1}    ${False}

    # Setup test result file
    ${current_time} =    Get Current Date    result_format=%Y%m%d-%H%M%S
    ${result_file} =    Set Variable    ${OUTPUT_DIR}${/}${RESULTE_FILE_NAME}_${current_time}.txt
    Set Global Variable    ${result_file}    ${result_file}
    Create File    path=${result_file}

    # Setup GPIO pairs for integrated antenna module. These pairs are specified as the CYW20820 port pins (P15, P8, etc.).
    # The first pin in the pair is the output pin and the second pin is the input pin.
    # 15 -> 8 : module pin 6 -> 7
    # 2 -> 3 : module pin 8 -> 9
    # 6 -> 17 : module pin 10 -> 11
    # 13 -> 9 : module pin 12 -> 13
    ${gpio_sets} =    Evaluate    [[15, 8], [2, 3], [6, 17], [13, 9]]    # TODO: Need to add all pairs
    Set Global Variable    ${GPIO_SETS}    ${gpio_sets}

Test Teardown
    De-Init Board    ${if820_board1}

Init Module Result
    ${module_result} =    Create List
    Set Global Variable    ${module_result}    ${module_result}

Log Result
    [Arguments]    ${result}
    Append To List    ${module_result}    ${result}

Program firmware
    [Timeout]    ${PROGRAM_FIRMWARE_TIMEOUT}
    [Arguments]    ${skip}=${False}
    IF    not ${skip}
        IF820 Flash Firmware    ${if820_board1}    ${MINI_DRIVER}    ${FIRMWARE}    ${True}
    END
    Init Board    ${if820_board1}    ${True}

Query Firmware Version
    [Timeout]    ${TEST_TIMEOUT_SHORT}
    ${res} =    IF820 Query Firmware Version    ${if820_board1}
    Log Result    ${res}
    Log    Firmware version: ${res}

Query Bluetooth Address
    [Timeout]    ${TEST_TIMEOUT_SHORT}
    ${res} =    IF820 Query Bluetooth Address String    ${if820_board1}
    Log Result    ${res}
    Log    Bluetooth address: ${res}

Test GPIO
    [Timeout]    ${PROGRAM_FIRMWARE_TIMEOUT}
    FOR    ${pins}    IN    @{GPIO_SETS}
        # Config output pin
        EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_SET_DRIVE}
        ...    pin=${pins[0]}
        ...    pin_config=${GPIO_CONFIG_OUTPUT}
        ...    pin_out_value=${0}
        ...    pin_operation=${GPIO_OP_IMMEDIATE}
        # Config input pin
        EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_SET_DRIVE}
        ...    pin=${pins[1]}
        ...    pin_config=${GPIO_CONFIG_INPUT}
        ...    pin_out_value=${0}
        ...    pin_operation=${GPIO_OP_IMMEDIATE}
        # Set and read state
        FOR    ${state}    IN    @{GPIO_TEST_STATES}
            EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_SET_LOGIC}
            ...    pin=${pins[0]}
            ...    pin_out_value=${state}
            ${res} =    EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_GET_LOGIC}
            ...    pin=${pins[1]}
            ...    direction=${0}
            Log    ${res}
            IF    ${res.payload.logic} != ${state}
                Fail    GPIO ${pins[0]} state ${res.payload.logic} should be ${state}
            END
        END
        # Release pins from control
        EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_SET_DRIVE}
        ...    pin=${pins[0]}
        ...    pin_config=${0}
        ...    pin_out_value=${0}
        ...    pin_operation=${GPIO_OP_RELEASE}
        EZ Send DUT1    ${lib_ez_serial_port.CMD_GPIO_SET_DRIVE}
        ...    pin=${pins[1]}
        ...    pin_config=${0}
        ...    pin_out_value=${0}
        ...    pin_operation=${GPIO_OP_RELEASE}
    END

Record Result
    Log    ${module_result}
    ${result_string} =    Set Variable    ${EMPTY}
    ${result_length} =    Get Length    ${module_result}
    FOR    ${index}    IN RANGE    ${result_length}
        ${value} =    Set Variable    ${module_result[${index}]}
        ${result_string} =    Set Variable    ${result_string}${value}
        IF    ${index} < ${result_length-1}
            ${result_string} =    Set Variable    ${result_string},
        ELSE
            ${result_string} =    Set Variable    ${result_string}${\n}
        END
    END
    Append To File    ${result_file}    ${result_string}

Manufacturing Test Loop
    WHILE    $True
        Init Module Result
        TRY
            ${barcode} =    Get Value From User    Enter Barcode
        EXCEPT
            Pass Execution    User stopped test
        END
        Log Result    ${barcode}
        Log    Barcode: ${barcode}
        Program firmware
        Query Bluetooth Address
        Query Firmware Version
        Test GPIO
        # TODO: Set TX power and start advertising
        # TODO: Measure TX power
        Record Result
    END
