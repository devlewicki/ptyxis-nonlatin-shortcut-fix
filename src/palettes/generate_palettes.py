#!/usr/bin/env python3

import os
import yaml

from gi.repository import GLib

_Palette = 'Palette'

for f in os.listdir(sys.argv[1]):
    if not f.endswith('.yml'):
        continue

    info = yaml.safe_load(open(f, 'r').read())

    keyfile = GLib.KeyFile.new()
    keyfile.set_string(_Palette, 'Name', info['name'])

    keyfile.set_string(_Palette, 'Background', info['background'])
    keyfile.set_string(_Palette, 'Foreground', info['foreground'])
    keyfile.set_string(_Palette, 'Cursor', info['cursor'])

    for i in range(0, 16):
        color = info['color_%02u' % (i+1)]
        keyfile.set_string(_Palette, 'Color%u' % i, color)

    data, _ = keyfile.to_data()

    with open(f.replace('.yml', '.palette'), 'w') as w:
        w.write(data)
        w.write('\n# This file was generated from https://github.com/Gogh-Co/Gogh/tree/master/themes/%s\n' % f)

