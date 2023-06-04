#!/usr/bin/env python3
import argparse
import os
import subprocess
import tempfile
from time import sleep
from pathlib import Path

import serial
from serial.serialutil import SerialException
from serial.tools import list_ports

from insert_serial import pad_serial, insert_bin_serial

BOARD_TYPE = 'MCv4B'
BOARD_VID = '0403'
BOARD_PID = '6001'
EEPROM_CONFIG = (Path(__file__).parent / '../utils/mcv4.conf.in').resolve()


def program_eeprom(asset):
    with tempfile.TemporaryDirectory() as tmpdirname:
        tmpdir = Path(tmpdirname)
        orig_dir = os.getcwd()
        try:
            config_data = EEPROM_CONFIG.read_text()

            # Apply asset code to eeprom config
            config_data = config_data.replace('SERIAL', asset)
            eeprom_conf = tmpdir / 'mcv4.conf'
            eeprom_conf.write_text(config_data)
            os.chdir(tmpdir)

            # Flash eeprom w/ ftdi_eeprom
            subprocess.check_call(['ftdi_eeprom', '--flash-eeprom', 'mcv4.conf'])

            Path('mcv4.eeprom').rename('mcv4-in.eeprom')
            # Read back eeprom to verify
            subprocess.check_call(['ftdi_eeprom', '--read-eeprom', 'mcv4.conf'])

            input_binary = Path('mcv4-in.eeprom').read_bytes()
            output_binary = Path('mcv4.eeprom').read_bytes()
            if input_binary == output_binary:
                print("EEPROM verified")
            else:
                print("Verification failed")
                raise RuntimeError

        finally:
            os.chdir(orig_dir)


def serial_flash_firmware(port, firmware, detect_asset=False):
    print(
        "To flash the board using the factory bootloader you need to connect "
        "12 volts to the board and connect the USB port. "
        "Once connected press the board's pushbutton")
    input('Press enter once this is done')

    if port is None:
        port = find_port()
        if port is None:
            print('Failed to find serial port')
            raise RuntimeError

    if detect_asset:
        asset_code = find_asset(port)
    else:
        asset_code = input("Enter asset code to bake into the firmware and USB descriptor: ")
        asset_code = 'sr' + asset_code.upper()
    print(f"Programming asset code: {asset_code}")

    with tempfile.TemporaryDirectory() as tmpdirname:
        tmpdir = Path(tmpdirname)
        # Apply asset code to fw
        stock_data = Path(firmware).read_bytes()
        serial_num_addr = stock_data.find(b'XXXXXXXXXXXXXXX')

        if serial_num_addr == -1:
            print("Couldn't find asset code placeholder")
            raise subprocess.CalledProcessError

        data = insert_bin_serial(stock_data, pad_serial(asset_code, 15), serial_num_addr)

        # write fw w/ serial num to temp file
        fw_file = tmpdir / 'main.bin'
        fw_file.write_bytes(data)

        # Flash fw w/ stm32flash
        subprocess.check_call(['stm32flash', '-b', '115200', '-w', str(fw_file), '-v', '-R', port])
    sleep(2)
    return asset_code


def verify_firmware(version, expected_serial=None):
    try:
        s = serial.serial_for_url(f'hwgrep://{BOARD_VID}:{BOARD_PID}', timeout=3, baudrate=115200)
        s.flush()
        s.write(b'*IDN?\n')
        identity_raw = s.readline()
        if not identity_raw.endswith(b'\n'):
            print(f"Board did not correctly respond to identify, returned: {identity_raw!r}")
            return False

        identity = identity_raw.decode('utf-8').strip().split(':')
        if identity[0] != 'Student Robotics':
            print("Incorrect manufacturer returned")
            return False
        if identity[1] != BOARD_TYPE:
            print("Incorrect board type returned")
            return False
        if identity[3] != version:
            print('Incorrect version returned')
            return False
        if expected_serial is not None and identity[2] != expected_serial:
            print(f"Serial number differs from expected, received {identity[2]!r}")
            return False

        print(f"Successfully flashed {identity[2]}")
        return identity[2]

    except SerialException:
        print("Failed to open serial port")
    return False


def log_success(log_file, asset_code):
    with open(log_file, 'a') as f:
        f.write(f"{asset_code}\n")


def find_port():
    try:
        s = serial.serial_for_url(f'hwgrep://{BOARD_VID}:{BOARD_PID}', do_not_open=True)
        return s.name
    except SerialException:
        return None


def find_asset(port):
    return next(list_ports.grep(port)).serial_number


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('file', help="Firmware file to flash")
    parser.add_argument('version', help="The version number the new firmware reports")
    parser.add_argument('-log', '--serial_log', default=None, help="File to store serials successfully flashed")
    parser.add_argument('--port', default=None, help="The serial port to access the board on, defaults to autodetection")
    parser.add_argument('--eeprom', action='store_true', help="Also program the FTDI EEPROM with the asset code")
    parser.add_argument('-auto', '--detect_asset', action='store_true', help="Autodetect asset code from eeprom")

    args = parser.parse_args()

    if args.eeprom and args.detect_asset:
        raise RuntimeError("Can't write eeprom and use it to detect asset code.")

    try:
        while True:
            input('Press enter to flash motor board')
            try:
                expected_asset = serial_flash_firmware(args.port, args.file, args.detect_asset)
            except subprocess.CalledProcessError:
                print("Failed to flash firmware, try again")
                continue

            verified_code = verify_firmware(args.version, expected_asset)
            if args.eeprom:
                try:
                    program_eeprom(expected_asset)
                except RuntimeError:
                    print("EEPROM write failed")
                    continue
            if verified_code is not False and args.serial_log is not None:
                log_success(args.serial_log, verified_code)
    except (KeyboardInterrupt, EOFError):
        return


if __name__ == '__main__':
    main()
