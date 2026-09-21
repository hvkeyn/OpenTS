# -*- coding: utf-8 -*-
"""Copy the build's own files into a game folder that already has the data.

    python assemble.py <build folder>

The build folder is expected to hold `TiberianSun` (and, if the mod is wanted,
`TwistedInsurrection`) with the retail or freeware data in it.
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DIST = os.path.dirname(HERE)


def copy_tree(src, dst):
    made = 0
    for dirpath, _dirs, names in os.walk(src):
        rel = os.path.relpath(dirpath, src)
        target = os.path.join(dst, rel) if rel != "." else dst
        os.makedirs(target, exist_ok=True)
        for n in names:
            shutil.copy2(os.path.join(dirpath, n), os.path.join(target, n))
            made += 1
    return made


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 1

    build = os.path.abspath(argv[0])
    ts = os.path.join(build, "TiberianSun")
    ti = os.path.join(build, "TwistedInsurrection")

    if not os.path.isdir(ts):
        print("there is no TiberianSun folder in", build)
        return 1

    print("copying into", ts)
    print("   files:", copy_tree(os.path.join(DIST, "game"), ts))

    if os.path.isdir(ti):
        print("copying into", ti)
        print("   files:", copy_tree(os.path.join(DIST, "game_ti"), ti))

    for name in ("Play.cmd", "КАК ИГРАТЬ.txt"):
        src = os.path.join(DIST, name)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(build, name))
            print("   copied", name)

    print()
    print("done. Game.exe has to be built from the repository and copied into the")
    print("game folders, then Play.cmd starts the game.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
