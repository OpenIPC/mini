#!/usr/bin/env python3

import sys
import os
import re


def make_hex(string):
    return re.sub(r'[^\dA-Fa-f]', '', string)


def make_good_name(manufacturer, hexcode):
    return (manufacturer + hexcode[:4] +
            "V" + hexcode[4:]).title()


# strings ./mpp/ko/hi3516cv300_sys.ko|grep "\[SYS\] Version: "
def find_sdk_version(filename, match):
    manufacturer = match[0][0]
    if manufacturer == 'hi':
        r = br'\[(?:SYS|ISP)\]\sVersion: \[(.+)(?:_MPP)_V(\d+)\.(\d+)\.([\d\w]+)\.(\d+) (.+ Release)*\].+\[(.+)\]'
    elif manufacturer == 'gk':
        r = br'\[ISP\]\sVersion: \[()ISP_V(\d+)\.(\d+)\.([\d\w]+)\.(\d+) (.+ Release)*\].+\[(.+)\]'
    else:
        return False
    pattern = re.compile(r)
    with open(filename, "rb") as f:
        ret = pattern.findall(f.read())
        if ret:
            matches = [x.decode() for x in ret[0]]
            hexcode = make_hex(matches[0])
            if hexcode == "":
                hexcode = make_hex(os.path.basename(filename))
                matches[0] = make_good_name(manufacturer, hexcode)
            print(hexcode, ";".join(matches), sep=';',
                  end='')
            return True
    return False


# traverse root directory, and list directories as dirs and files as files
def find_sys_ko(path):
    pattern = re.compile(r"(\w\w)[0-9a-z]+_(sys|isp).ko")
    for root, dirs, files in os.walk(path):
        path = root.split(os.sep)
        for file in files:
            match = pattern.findall(file)
            if match:
                if find_sdk_version(os.path.join(root, file), match):
                    return


def main():
    if len(sys.argv) != 2:
        root_dir = os.getcwd()
    else:
        root_dir = sys.argv[1]
    find_sys_ko(root_dir)


if __name__ == '__main__':
    main()
