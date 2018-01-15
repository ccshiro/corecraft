#!/usr/bin/env python

"""Script to sort #includes and forward declares.

Tailored to work on our server software and might need editing to get it to work
elsewhere.

Sorts includes in the following order:
1. header files with same name as code file
2. header files enclosed in quotes with no slash in them
3. header files enclosed in quotes with a slash in them
4. header files starting with <something/
5. header files starting with <
Note: all categories are internally lexiographically sorted on the full path.

The include block is put where the first include was found. Puts all top-level
forward declares right under the first include block and sorts them
lexiographically. Will currently not do anything with forward declares in other
namespaces.
"""

import argparse
import copy
import os
import sys

def sort_incls(f):
    ext = os.path.splitext(f.name)[1]
    if ext not in ['.h', '.hpp', '.cpp', '.c', '.cc']:
        sys.stderr.write(f.name + " has an invalid file extension\n")
        return

    lines = f.readlines()

    incl_block_begin = -1
    incl_block_end = -1
    module_header_name = ''
    if ext in ['.cpp', '.c', '.cc']:
        module_header_name = os.path.splitext(os.path.basename(f.name))[0]
    prev_line_comment = False
    prev_line_template = False
    erase_lines = []
    line_end = -1 # use same line ending as first line of file
    braces_counter = 0
    includes_whitespace = False # attempt to patch whitespace gaps in include
                                # block, fails on any non-newline character

    module_header = ''     # 1.
    local_headers = []     # 2.
    proj_headers = []      # 3.
    sys_slash_headers = [] # 4.
    sys_headers = []       # 5.
    fwd_struct_decls = []
    fwd_class_decls = []

    for (i, l) in enumerate(lines):
        if line_end == -1:
            line_end = '\r\n' if l.endswith('\r\n') else '\n'
        if (incl_block_end == -1 and not l.startswith('#include') and
            incl_block_begin != -1):
            incl_block_end = i
        l.strip()
        if l.find('{') != -1:
            braces_counter = braces_counter + 1
        elif l.find('}') != -1:
            braces_counter = braces_counter - 1
        if (includes_whitespace and not l.startswith('#include') and
            l != line_end):
            includes_whitespace = False
        if l.startswith('#include'):
            if l.find("// sort off") != -1:
                if incl_block_begin != -1 and incl_block_end == -1:
                    sys.stderr.write(f.name + " has ignored include inside " +
                        " of the include block\n")
                    return
                continue # ignore sorting
            if prev_line_comment:
                sys.stderr.write(f.name + " has comments in the include block\n")
                return
            if incl_block_end != -1:
                if includes_whitespace:
                    incl_block_end = -1
                else:
                    sys.stderr.write(f.name + " has includes not part of " +
                        "include block\n")
                    return
            if incl_block_begin == -1:
                includes_whitespace = True
                incl_block_begin = i
            header = l[len('#include'):].lstrip()
            if module_header_name and module_header_name in header:
                module_header = header
                module_header_name = ''
            elif header.startswith('<') and '/' in header:
                sys_slash_headers.append(header)
            elif header.startswith('<'):
                sys_headers.append(header)
            elif '/' in header:
                proj_headers.append(header)
            else:
                local_headers.append(header)
        elif (l.startswith('struct') and l.find(';') != -1 and
            braces_counter == 0 and not prev_line_template):
            if incl_block_begin != -1 and incl_block_end == -1:
                sys.stderr.write(f.name + " has fwd decls before end of " +
                    "include block\n")
                return
            if l.find("// sort off") != -1:
                continue # ignore sorting
            if prev_line_comment:
                sys.stderr.write(f.name + " has comment before fwd declare\n")
                return
            fwd_struct_decls.append(l[len('struct'):].split(';', 1)[0].strip())
            erase_lines.append(l)
        elif (l.startswith('class') and l.find(';') != -1 and
            braces_counter == 0 and not prev_line_template):
            if incl_block_begin != -1 and incl_block_end == -1:
                sys.stderr.write(f.name + " has fwd decls before end of " +
                    "include block\n")
                return
            if l.find("// sort off") != -1:
                continue # ignore sorting
            if prev_line_comment:
                sys.stderr.write(f.name + " has comment before fwd declare\n")
                return
            fwd_class_decls.append(l[len('class'):].split(';', 1)[0].strip())
            erase_lines.append(l)

        # Don't change lines that have a preceeding C++ style comment; they are
        # most likely important for the following line. We ignore C-style
        # comments, however.
        prev_line_comment = l.startswith('//')
        prev_line_template = l.find('template') != -1

    # cannot process a file without includes
    if incl_block_end == -1:
        if fwd_struct_decls or fwd_class_decls:
            sys.stderr.write(f.name + " has forward declares but no include " +
                "block\n")
        return

    prev_lines = copy.copy(lines)

    # erase all fwd declares
    lines[:] = [x for x in lines if x not in erase_lines]

    # sort and write include & fwd decl block
    module_headers = [module_header] if module_header else []
    local_headers = sorted(set(local_headers))
    proj_headers = sorted(set(proj_headers))
    sys_slash_headers = sorted(set(sys_slash_headers))
    sys_headers = sorted(set(sys_headers))
    fwds = ['struct ' + d for d in fwd_struct_decls]
    fwds.extend(['class ' + d for d in fwd_class_decls])
    def fwd_decl_key(x):
        return (x[len('class'):].lstrip() if x.startswith('class') else
            x[len('struct'):].lstrip())
    fwds = sorted(set(fwds), key=fwd_decl_key)
    fwds = ['{0};'.format(e) + line_end for e in fwds]
    headers = (module_headers + local_headers + proj_headers + sys_slash_headers
        + sys_headers)
    block_lines = ['#include ' + h for h in headers]
    block_lines.append(line_end)
    if fwds:
        block_lines.extend(fwds)

    lines = lines[:incl_block_begin] + block_lines + lines[incl_block_end+1:]

    if lines != prev_lines:
        f.seek(0)
        f.truncate()
        f.writelines(lines)

def main():
    parser = argparse.ArgumentParser(description="Sort includes and " +
        "fwd-declares")
    parser.add_argument('files', nargs='+', type=argparse.FileType('r+'),
        help="the source files to operate on")
    args = parser.parse_args()
    for f in args.files:
        sort_incls(f)

if __name__ == "__main__":
    main()
