*** Settings ***
Library           Process
Library           OperatingSystem
Library           scripts/UartLibrary.py

Suite Setup       Log To Console    Starting hardware unit test pipeline...
Suite Teardown    Log To Console    Hardware unit test pipeline finished.

*** Variables ***
${BUILD_DIR}          build-ra2e3
${PORT}               COM7
# The on-target Unity port (tests/target/unity_port_polled_gpio.c) is a polled
# software UART running at 9600 baud, NOT 115200.
${BAUD}               9600
${TIMEOUT_SEC}        60
${PROGRAM_TARGET}     rfp_program_ut
${ERASE_TARGET}       rfp_erase_halt
${BLANKCHECK_TARGET}  rfp_check_blank
${DO_ERASE}           0
${DO_BLANKCHECK}      0

*** Test Cases ***
Run On-Target Unity Firmware Tests
    Maybe Erase Device
    Maybe Check Blank
    Program Unit Test Firmware
    ${log}=    Capture Unity Output After Reset    ${PORT}    ${BAUD}    ${TIMEOUT_SEC}    ${BUILD_DIR}
    Log To Console    \n===== UNITY LOG BEGIN =====\n${log}\n===== UNITY LOG END =====

*** Keywords ***
Maybe Erase Device
    IF    '${DO_ERASE}' == '1'
        ${result}=    Run Process    ninja    -C    ${BUILD_DIR}    ${ERASE_TARGET}    shell=${False}
        Should Be Equal As Integers    ${result.rc}    0    msg=Erase failed:\n${result.stdout}\n${result.stderr}
    END

Maybe Check Blank
    IF    '${DO_BLANKCHECK}' == '1'
        ${result}=    Run Process    ninja    -C    ${BUILD_DIR}    ${BLANKCHECK_TARGET}    shell=${False}
        Should Be Equal As Integers    ${result.rc}    0    msg=Blank check failed:\n${result.stdout}\n${result.stderr}
    END

Program Unit Test Firmware
    ${result}=    Run Process    ninja    -C    ${BUILD_DIR}    ${PROGRAM_TARGET}    shell=${False}
    Should Be Equal As Integers    ${result.rc}    0    msg=Programming failed:\n${result.stdout}\n${result.stderr}
