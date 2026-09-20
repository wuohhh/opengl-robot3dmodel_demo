# -*- coding: utf-8 -*-
"""
移植脚本 2/2：把 3dmodel/ 下原项目源码的 include 改写为 demo 适配层。

原则：只改 include，业务逻辑一行不动。改写后由 demo/3dmodel/demo_support.h
一次性提供原项目这 5 个头文件所声明的全部内容。
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL_DIR = os.path.join(HERE, '3dmodel')

# 原项目头文件 -> demo 适配层头文件
REPLACE_MAP = {
    'base/basedefine.h': 'demo_support.h',
    'base/userpushbutton.h': 'demo_support.h',
    'commu/commdatacontroller.h': 'demo_support.h',
    'robotconfig/robotconfig.h': 'demo_support.h',
    'base/softwareui_controller.h': 'demo_support.h',
    'robotAlgorithm/CoordTrans.h': 'demo_support.h',
    'robotAlgorithm/robotalginterface.h': 'demo_support.h',
}

TARGETS = [
    'robot3dglwidget.cpp', 'robot3dglwidget.h',
    'robot3dcontrolwidget.cpp', 'robot3dcontrolwidget.h',
    'robotgeometry.cpp', 'robotgeometry.h',
]


def process(path):
    with open(path, 'rb') as f:
        raw = f.read()
    has_bom = raw[:3] == b'\xef\xbb\xbf'
    text = raw.decode('utf-8-sig' if has_bom else 'utf-8')
    lines = text.replace('\r\n', '\n').split('\n')

    changed = []
    out = []
    for line in lines:
        newline = line
        for old, new in REPLACE_MAP.items():
            for quote in ('"', '<',):
                pass
            cands = ['#include"%s"' % old, '#include "%s"' % old, '#include<%s>' % old, '#include <%s>' % old]
            for cand in cands:
                if cand in newline:
                    newline = newline.replace(cand, '#include "%s"' % new)
        if newline != line:
            changed.append((line.strip(), newline.strip()))
        out.append(newline)

    # 去重：同一文件可能出现多行相同的 demo_support.h include
    seen = False
    dedup = []
    for line in out:
        if line.strip() == '#include "demo_support.h"':
            if seen:
                continue
            seen = True
        dedup.append(line)

    text = '\r\n'.join(dedup)
    with open(path, 'wb') as f:
        f.write((b'\xef\xbb\xbf' if has_bom else b'') + text.encode('utf-8'))
    return changed


def main():
    total = 0
    for name in TARGETS:
        path = os.path.join(MODEL_DIR, name)
        if not os.path.isfile(path):
            print('MISS %s' % path)
            continue
        changed = process(path)
        print('== %s  (改写 %d 行)' % (name, len(changed)))
        for old, new in changed:
            print('     - %s' % old)
            print('     + %s' % new)
        total += len(changed)
    print('-' * 60)
    print('共改写 %d 行 include' % total)
    return 0


if __name__ == '__main__':
    sys.exit(main())
