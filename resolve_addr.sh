#!/bin/bash
addr2line -a -i -p -s -f -C -e ./omamori_embedded_debug.elf "$@"

