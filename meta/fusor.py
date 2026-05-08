#!/usr/bin/env python
#
# script to amalgamate several individual headers into one single-header for
# easier distribution
#

import re
import os
import argparse
from pathlib import Path

__version__ = "0.3.2"

class Header:
    def __init__(self, name, uuid, lines):
        self.name  = name
        self.uuid  = uuid
        self.lines = lines


def strip_lines(lines, arg_str):
    """Strip lines [start, end) from the content."""
    args = arg_str.strip().split()
    if len(args) != 2:
        raise RuntimeError("strip_lines requires two integer arguments")
    if not (args[0].isdecimal() and args[1].isdecimal()):
        raise RuntimeError("strip_lines requires two integer arguments")
    start = int(args[0])
    end   = int(args[1])
    return lines[:start] + lines[end-1:]


def process_import_statements(lines):
    out_lines = []
    pattern = r'(.*)@ncr-fusor-import\s+(\S+)(?:\s+(\w+)(.*))?'
    r = re.compile(pattern)

    function_map = {
        'strip_lines': strip_lines
    }

    for line in lines:
        match = r.match(line)
        if match:
            prefix        = match.group(1)
            import_file   = match.group(2)
            function_name = match.group(3)
            args          = match.group(4)

            ## Load the content of the specified import file
            if os.path.isfile(import_file):
                with open(import_file, 'r') as import_file_handle:
                    import_lines = import_file_handle.readlines()

                if function_name and function_name in function_map:
                    import_lines = function_map[function_name](import_lines, args)

                for im_line in import_lines:
                    out_lines.append(f"{prefix}{im_line}")
        else:
            out_lines.append(line)

    return out_lines


def contains_header(ncr_headers, name, uuid):
    for hdr in ncr_headers:
        if (hdr.name == name) and (hdr.uuid == uuid):
            return True
    return False


def recurse_header(header_path, include_root, ncr_headers, std_headers, always_keep_includes=False):
    header_path = header_path.resolve()
    header_name = header_path.name
    with open(header_path, 'r') as file:
        lines = file.readlines()

    # find the UUID for identification
    uuid_pattern = r'#define _([a-fA-F0-9]{32})_'
    r_uuid = re.compile(uuid_pattern)
    uuid = None
    for line in lines:
        match = r_uuid.match(line)
        if match:
            uuid = match.group(1)
            break
    if not uuid:
        raise RuntimeError("Header files must have a UUID")

    # already processed? will also break infinite recursions
    if contains_header(ncr_headers, header_name, uuid):
        return []

    # list of all new headers processed here
    new_headers = []

    # determine if we should take this header as-is
    copy_as_is_pattern = r'.*@ncr-fusor-copy-file-as-is.*'
    r_as_is = re.compile(copy_as_is_pattern)
    copy_as_is = False
    for line in lines:
        match = r_as_is.match(line)
        if match:
            copy_as_is = True
            break
    if copy_as_is:
        hdr = Header(header_name, uuid, lines)
        ncr_headers.append(hdr)
        new_headers.append(hdr)
        return new_headers


    # find all includes
    include_pattern             = r'\s*#include\s*(["<])(.*?)([">])'
    keep_includes_start_pattern = r'.*@ncr-fusor-keep-includes-start.*'
    keep_includes_end_pattern   = r'.*@ncr-fusor-keep-includes-end.*'

    r_inc        = re.compile(include_pattern)
    r_keep_start = re.compile(keep_includes_start_pattern)
    r_keep_end   = re.compile(keep_includes_end_pattern)

    new_lines     = []
    keep_std_includes = False
    for line in lines:
        # test if we should keep the std include or not
        match = r_keep_start.match(line)
        if match:
            keep_std_includes = True
        match = r_keep_end.match(line)
        if match:
            keep_std_includes = False

        # process
        match = r_inc.match(line)
        if not match:
            new_lines.append(line)
        else:
            # standard header?
            inc_open        = match.group(1)
            dep_header_name = match.group(2)
            inc_close       = match.group(3)
            is_ncr_header   = inc_open == '"' and inc_close == '"'
            if is_ncr_header:
                # recurse into header dependency
                # old, all relative: dep_header_path = header_path.parent / dep_header_name
                dep_header_path = include_root / dep_header_name
                hdrs = recurse_header(dep_header_path, include_root, ncr_headers, std_headers, always_keep_includes)
                new_headers.extend(hdrs)
            else:
                # standard headers will be collected separately
                if keep_std_includes or always_keep_includes:
                    new_lines.append(line)
                elif dep_header_name not in std_headers:
                    std_headers.append(dep_header_name)
                    print(dep_header_name, header_path)

    if new_lines[-1] != '\n':
        new_lines.append('\n')
    hdr = Header(header_name, uuid, new_lines)
    ncr_headers.append(hdr)
    new_headers.append(hdr)

    return new_headers


def strip_stuff(lines):
    patterns = [
            r'\s*#include\s*"(.*?)".*',
            r'.*SPDX-.*',
            r'.*See LICENSE for more details.*',
            r'.*See LICENSE file for more details.*'
    ]

    rs = [re.compile(pattern) for pattern in patterns]
    out_lines = []

    for line in lines:
        match = any(r.match(line) for r in rs)
        if not match:
            out_lines.append(line)

    return out_lines


def process_ncr_includes(lines, include_root, ncr_headers, std_headers, keep_includes=False):
    out_lines = []
    fusor_include_op = r'//.*@ncr-fusor-include\s*"(.*?)"'
    r = re.compile(fusor_include_op)

    for line in lines:
        match = r.match(line)
        if not match:
            out_lines.append(line)
        else:
            # Resolves the entry point (e.g., "ncr/header.hpp") from the root
            header_path = include_root / match.group(1)

            # need to pass the include_root so that we can recurse into files
            new_hdrs = recurse_header(header_path, include_root, ncr_headers, std_headers, always_keep_includes=keep_includes)

            for hdr in new_hdrs:
                hdr_lines = strip_stuff(hdr.lines)

                comment_line = f'// {hdr.name} //'
                comment_line = comment_line.ljust(80, '/') + '\n'
                out_lines.extend(comment_line)
                out_lines.extend(hdr_lines)

    return out_lines


def process_stdheaders(lines, stdheaders):
    r = re.compile('.*@ncr-fusor-stdheaders.*')
    out_lines = []
    for line in lines:
        if not r.match(line):
            out_lines.append(line)
        else:
            for hdr in stdheaders:
                out_lines.append(f'#include <{hdr}>\n')
    return out_lines


def process_strip(lines):
    r_start = re.compile('.*@ncr-fusor-strip-start.*')
    r_end   = re.compile('.*@ncr-fusor-strip-end.*')
    out_lines = []
    keep = True
    for line in lines:
        if r_start.match(line):
            keep = False
            continue
        if r_end.match(line):
            keep = True
            continue
        if keep:
            out_lines.append(line)
    return out_lines


def strip_ncr_fusor_ops(lines):
    r = re.compile('.*@ncr-fusor-.*')
    out_lines = []
    for line in lines:
        if not r.match(line):
            out_lines.append(line)
    return out_lines


def main(args):
    fname_in, fname_out = args.filename_in, args.filename_out
    with open(fname_in, 'r') as file:
        lines = file.readlines()

    ncr_headers, std_headers = [], []
    include_root = Path(args.include_root).resolve()

    lines = process_import_statements(lines)
    lines = process_ncr_includes(lines, include_root, ncr_headers, std_headers, args.keep_includes)
    lines = process_stdheaders(lines, std_headers)
    lines = process_strip(lines)
    lines = strip_ncr_fusor_ops(lines)

    with open(fname_out, 'w') as file:
        for line in lines:
            file.write(line)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="fuse headers based on a .hpp.in file")
    parser.add_argument('filename_in', type=str, help='The fusor-template .hpp.in file')
    parser.add_argument('filename_out', type=str, help='The output file name. Will overwrite if file exists')
    parser.add_argument('-I', '--include-root', default=Path('../include'), help='Include root directory')
    parser.add_argument('-k', '--keep-includes', default=False, action='store_true', help='Always keep include statements')
    parser.add_argument('-v', '--version', action='version', version=f"%(prog)s {__version__}")
    args = parser.parse_args()
    main(args)
