*** Settings ***
Documentation       IF820 DVK Manufacturing Test
...                 This test will test one IF820 DVK at a time.

Resource            common.robot

Default Tags        vela if820


*** Variables ***
${PROBE_SETTING_TARGET_DEVICE_VENDOR}=      ARM
${PROBE_SETTING_TARGET_DEVICE_NAME}=        cortex_m
${PROBE_SETTING_BOARD_VENDOR}=              Ezurio
${PROBE_SETTING_BOARD_NAME}=                Vela IF820 DVK
${PROBE_SETTING_USB_VID}=                   ${0x3016}
${PROBE_SETTING_USB_PID}=                   ${0x0009}


*** Test Cases ***
DVK MFG Test
    Find Boards and Settings

    Init Board    ${if820_board1}    ${False}

    Call Method
    ...    ${if820_board1.probe}
    ...    program_v2_settings
    ...    ${PROBE_SETTING_BOARD_VENDOR}
    ...    ${PROBE_SETTING_BOARD_NAME}
    ...    ${PROBE_SETTING_TARGET_DEVICE_VENDOR}
    ...    ${PROBE_SETTING_TARGET_DEVICE_NAME}
    ...    ${PROBE_SETTING_USB_VID}
    ...    ${PROBE_SETTING_USB_PID}

    De-Init Board    ${if820_board1}
