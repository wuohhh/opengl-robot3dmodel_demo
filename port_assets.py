# -*- coding: utf-8 -*-
"""
移植脚本 1/2：从原项目拷贝 3D 模块源码、shader、图标与模型资源到 demo 目录。
本机存在透明加密系统，Python 在进程白名单内，读写均为明文 -> 落盘即明文，便于后续查看/编辑/编译。
"""
import os
import shutil
import sys

SRC_ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DST_ROOT = os.path.abspath(os.path.dirname(os.path.abspath(__file__)))

# ---------- 需要移植的源码（原样搬运，后续由 port_code.py 做最小化 include 改写） ----------
CODE_FILES = [
    r'move3D\model3D\robot3dglwidget.cpp',
    r'move3D\model3D\robot3dglwidget.h',
    r'move3D\model3D\robot3dcontrolwidget.cpp',
    r'move3D\model3D\robot3dcontrolwidget.h',
    r'move3D\model3D\robotgeometry.cpp',
    r'move3D\model3D\robotgeometry.h',
    r'move3D\model3D\basegeometry.cpp',
    r'move3D\model3D\basegeometry.h',
    r'move3D\model3D\linegeometry.cpp',
    r'move3D\model3D\linegeometry.h',
    r'move3D\model3D\spheregeometry.cpp',
    r'move3D\model3D\spheregeometry.h',
    r'move3D\model3D\planegeometry.cpp',
    r'move3D\model3D\planegeometry.h',
    r'move3D\model3D\substancegeometry.cpp',
    r'move3D\model3D\substancegeometry.h',
]

# ---------- shader ----------
SHADER_FILES = [
    'line_fragment_shader.glsl', 'line_vertex_shader.glsl',
    'plane_fragment_shader.glsl', 'plane_vertex_shader.glsl',
    'robot_fragment_shader.glsl', 'robot_vertex_shader.glsl',
    'substance_fragment_shader.glsl', 'substance_vertex_shader.glsl',
]

# ---------- 3D 控件用到的图标（由 robot3dglwidget::InitUI 引用） ----------
ICON_FILES = [
    'zoomin.png', 'zoomout.png', 'recovery3d.png', 'R_T_change.png', 'T_R_change.png',
    'end_tool_hide.png', 'end_tool_show.png',
    'machineBase_checked.png', 'machineBase_unchecked.png',
    'targetPose_checked.png', 'targetPose_unchecked.png',
    'currentPose_checked.png', 'currentPose_unchecked.png',
    'waypointPose_checked.png', 'waypointPose_unchecked.png',
    'pathCurve_checked.png', 'pathCurve_unchecked.png',
]

# ---------- 需要拷贝的机型（每个机型含 base/joint1..6(.tool) 的 obj+mtl） ----------
ROBOT_TYPES = ['G6L-3-20', 'G6-1-1', 'X20-1-3']


def ensure_dir(path):
    if not os.path.isdir(path):
        os.makedirs(path)


def copy_text(src, dst):
    """按原编码读出（透明解密），统一写成 UTF-8 BOM + CRLF，保证 Windows 下编辑器识别中文。"""
    with open(src, 'rb') as f:
        raw = f.read()
    if raw[:3] == b'\xef\xbb\xbf':
        try:
            text = raw.decode('utf-8-sig')
        except UnicodeDecodeError:
            text = raw.decode('utf-8', 'replace')
    else:
        try:
            text = raw.decode('utf-8')
        except UnicodeDecodeError:
            text = raw.decode('gbk', 'replace')
    text = text.replace('\r\n', '\n').replace('\r', '\n').replace('\n', '\r\n')
    ensure_dir(os.path.dirname(dst))
    with open(dst, 'wb') as f:
        f.write(b'\xef\xbb\xbf' + text.encode('utf-8'))
    return len(raw), os.path.getsize(dst)


def copy_binary(src, dst):
    """二进制资源：Python 透明读取明文，直接写出。"""
    ensure_dir(os.path.dirname(dst))
    with open(src, 'rb') as f:
        data = f.read()
    with open(dst, 'wb') as f:
        f.write(data)
    return len(data), os.path.getsize(dst)


def main():
    if not os.path.isfile(os.path.join(SRC_ROOT, 'CGX_TP.pro')):
        sys.stderr.write('找不到原项目根目录: %s\n' % SRC_ROOT)
        return 1

    report = []

    # 1. 源码
    for rel in CODE_FILES:
        src = os.path.join(SRC_ROOT, rel)
        dst = os.path.join(DST_ROOT, '3dmodel', os.path.basename(rel))
        a, b = copy_text(src, dst)
        report.append(('SRC ', rel, a, b))

    # 2. shader
    for name in SHADER_FILES:
        src = os.path.join(SRC_ROOT, 'shaders', name)
        dst = os.path.join(DST_ROOT, 'shaders', name)
        a, b = copy_text(src, dst)
        report.append(('SHDR', name, a, b))

    # 3. 图标
    for name in ICON_FILES:
        src = os.path.join(SRC_ROOT, 'resource', 'image', name)
        dst = os.path.join(DST_ROOT, 'resource', 'image', name)
        a, b = copy_binary(src, dst)
        report.append(('ICON', name, a, b))

    # 4. 机型模型
    for robot in ROBOT_TYPES:
        src_dir = os.path.join(SRC_ROOT, 'robotType', robot, 'model3d')
        if not os.path.isdir(src_dir):
            report.append(('MISS', robot, 0, 0))
            continue
        for name in sorted(os.listdir(src_dir)):
            if not name.lower().endswith(('.obj', '.mtl')):
                continue
            src = os.path.join(src_dir, name)
            dst = os.path.join(DST_ROOT, 'robotType', robot, 'model3d', name)
            a, b = copy_binary(src, dst)
            report.append(('MODL', '%s/%s' % (robot, name), a, b))
        # 机型配置（DH 参数来源，demo 直接读它）
        cfg_src = os.path.join(SRC_ROOT, 'robotType', robot, 'config.xml')
        if os.path.isfile(cfg_src):
            cfg_dst = os.path.join(DST_ROOT, 'robotType', robot, 'config.xml')
            a, b = copy_binary(cfg_src, cfg_dst)
            report.append(('CONF', '%s/config.xml' % robot, a, b))

    # 5. assimp 运行库（MinGW 版）
    assimp_src = os.path.join(SRC_ROOT, 'move3D', 'model3D', 'assimp', 'MinGW', 'libassimp.dll')
    if os.path.isfile(assimp_src):
        a, b = copy_binary(assimp_src, os.path.join(DST_ROOT, 'bin', 'libassimp.dll'))
        report.append(('DLL ', 'libassimp.dll', a, b))

    # 6. assimp 头文件（编译必需）
    inc_src = os.path.join(SRC_ROOT, 'move3D', 'model3D', 'assimp', 'include')
    inc_dst = os.path.join(DST_ROOT, 'assimp', 'include')
    count = 0
    for root, dirs, files in os.walk(inc_src):
        for name in files:
            s = os.path.join(root, name)
            rel = os.path.relpath(s, inc_src)
            d = os.path.join(inc_dst, rel)
            ensure_dir(os.path.dirname(d))
            shutil.copyfile(s, d)  # 头文件为明文，直接复制
            count += 1
    report.append(('INC ', 'assimp/include', count, count))

    # 7. 导入库（MinGW 版）
    implib = os.path.join(SRC_ROOT, 'move3D', 'model3D', 'assimp', 'MinGW', 'libassimp.dll.a')
    if os.path.isfile(implib):
        ensure_dir(os.path.join(DST_ROOT, 'assimp', 'lib'))
        a, b = copy_binary(implib, os.path.join(DST_ROOT, 'assimp', 'lib', 'libassimp.dll.a'))
        report.append(('LIB ', 'libassimp.dll.a', a, b))

    print('%-5s %-46s %10s %10s' % ('KIND', 'FILE', 'SRC', 'DST'))
    print('-' * 78)
    for kind, name, a, b in report:
        print('%-5s %-46s %10d %10d' % (kind, name[:46], a, b))
    print('-' * 78)
    print('共 %d 项' % len(report))
    return 0


if __name__ == '__main__':
    sys.exit(main())
