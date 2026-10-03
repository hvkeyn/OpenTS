# -*- coding: utf-8 -*-
r"""Собирает патч, которым старая копия сборки обновляется до последнего релиза.

    python make_patch.py

Патч нужен тому, у кого уже лежит копия сборки: качать 2,5 ГБ заново незачем,
достаточно заменить то, что изменилось. Внутрь кладётся:

  * движок и Language.dll из ОПУБЛИКОВАННОГО релиза, а не из локальной сборки:
    так копия совпадает с тем, что поставит себе автообновление;
  * файлы, которые сверяет Проверка.cmd (шрифты, списки кампаний, брифинги,
    полигоны, суперсилы) - из dist\game и dist\game_ti;
  * файлы запуска и обновления из dist\ и установщик из dist\tools;
  * инструкция Прочитать.txt.

Ожидания в Проверка.cmd и Список_файлов.txt переписываются под релизный движок
и его Language.dll: иначе проверка сразу ругалась бы на только что установленное.
Файлы .cmd и .ps1 записываются с CRLF и BOM там, где он был: cmd.exe с одними
LF теряет разбор строки и выполняет куски строк как команды.
"""
import hashlib
import io
import json
import os
import re
import shutil
import sys
import unicodedata
import urllib.request
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
DIST = os.path.dirname(HERE)
BASE = os.path.dirname(os.path.dirname(DIST))
REPO = "hvkeyn/OpenTS"
WORK = os.path.join(BASE, "_patch_build")
OUT_NAME = "patch_OpenTS_%s_for_old_build.zip"


def latest_release():
    url = "https://api.github.com/repos/%s/releases/latest" % REPO
    request = urllib.request.Request(url, headers={"User-Agent": "OpenTS-RU-patch"})
    with urllib.request.urlopen(request, timeout=30) as answer:
        return json.load(answer)


def engine_pair():
    release = latest_release()
    tag = release["tag_name"]
    asset = next((a for a in release["assets"] if a["name"].endswith("Win32.zip")), None)
    if asset is None:
        raise SystemExit("в релизе %s нет сборки Win32" % tag)

    os.makedirs(WORK, exist_ok=True)
    cached = os.path.join(WORK, asset["name"])
    if not os.path.isfile(cached):
        print("   скачиваю", asset["name"])
        urllib.request.urlretrieve(asset["browser_download_url"], cached)

    with zipfile.ZipFile(cached) as z:
        exe = next((n for n in z.namelist() if n.lower().endswith("game.exe")), None)
        dll = next((n for n in z.namelist() if n.lower().endswith("language.dll")), None)
        if exe is None:
            raise SystemExit("в сборке релиза нет Game.exe")
        return tag, z.read(exe), (z.read(dll) if dll else None)


def load_bytes(rel):
    return io.open(os.path.join(DIST, rel), "rb").read()


def load_text(rel):
    raw = load_bytes(rel)
    return raw.decode("utf-8-sig"), raw[:3] == b"\xef\xbb\xbf"


def write_bytes(path, data):
    folder = os.path.dirname(path)
    if folder:
        os.makedirs(folder, exist_ok=True)
    io.open(path, "wb").write(data)


def write_text(path, text, bom):
    text = unicodedata.normalize("NFC", text)
    data = text.replace("\r\n", "\n").replace("\n", "\r\n").encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    write_bytes(path, data)


def checked_files():
    checker, _ = load_text("Проверка.cmd")
    pairs = re.findall(r'"%ROOT%([^"]+)"\)\s*do if not "%%~zA"=="(\d+)"', checker)
    out = {}
    for rel, _size in pairs:
        rel = rel.replace("\\\\", "\\")
        if rel.endswith("Game.exe") or rel.endswith("Language.dll") or rel.endswith("SUN.INI"):
            continue
        if rel.startswith("TiberianSun\\"):
            out[rel] = os.path.join("game", rel[len("TiberianSun\\"):])
        elif rel.startswith("TwistedInsurrection\\"):
            out[rel] = os.path.join("game_ti", rel[len("TwistedInsurrection\\"):])
    return out


def build():
    global OUT
    tag, engine, dll = engine_pair()
    OUT = os.path.join(BASE, OUT_NAME % tag)
    engine_md5 = hashlib.md5(engine).hexdigest()
    print("   релиз %s: движок %d байт, строки %s" %
          (tag, len(engine), len(dll) if dll else "нет"))

    patch = os.path.join(WORK, "patch")
    shutil.rmtree(patch, ignore_errors=True)
    os.makedirs(patch)

    for rel, source in checked_files().items():
        write_bytes(os.path.join(patch, rel), load_bytes(source))

    for game in ("TiberianSun", "TwistedInsurrection"):
        write_bytes(os.path.join(patch, game, "Game.exe"), engine)
        if dll:
            write_bytes(os.path.join(patch, game, "Language.dll"), dll)

    play, play_bom = load_text("Play.cmd")
    play = re.sub(r"ENGINE_SIZE=\d+", "ENGINE_SIZE=%d" % len(engine), play)
    write_text(os.path.join(patch, "Play.cmd"), play, play_bom)

    check, check_bom = load_text("Проверка.cmd")
    for game in ("TiberianSun", "TwistedInsurrection"):
        for name, data in (("Game.exe", engine), ("Language.dll", dll)):
            if data is None:
                continue
            rel = game + "\\" + name
            check = re.sub(r'("%ROOT%' + re.escape(rel) + r'"\)\s*do if not "%%~zA"==")\d+(")',
                           r"\g<1>%d\g<2>" % len(data), check)
    write_text(os.path.join(patch, "Проверка.cmd"), check, check_bom)

    listing, listing_bom = load_text("Список_файлов.txt")
    for game in ("TiberianSun", "TwistedInsurrection"):
        for name, data in (("Game.exe", engine), ("Language.dll", dll)):
            if data is None:
                continue
            rel = game + "\\" + name
            pattern = re.compile(r"^(" + re.escape(rel) + r"\s+)\d+(\s+\u0431\u0430\u0439\u0442\s+md5\s+)[0-9a-f]{32}", re.M)
            listing = pattern.sub(
                lambda m: m.group(1) + str(len(data)) + m.group(2) + hashlib.md5(data).hexdigest(),
                listing)
    write_text(os.path.join(patch, "Список_файлов.txt"), listing, listing_bom)

    for name in ("\u041a\u0430\u043a_\u0438\u0433\u0440\u0430\u0442\u044c.txt", "README.md", "update.ps1",
                 "\u041e\u0431\u043d\u043e\u0432\u0438\u0442\u044c.cmd"):
        if os.path.isfile(os.path.join(DIST, name)):
            write_bytes(os.path.join(patch, name), load_bytes(name))
    write_text(os.path.join(WORK, "VERSION.txt"), tag, False)

    for name in ("\u0423\u0441\u0442\u0430\u043d\u043e\u0432\u0438\u0442\u044c_\u043f\u0430\u0442\u0447.cmd", "install.ps1", "\u041f\u0440\u043e\u0447\u0438\u0442\u0430\u0442\u044c.txt"):
        source = os.path.join(HERE, name)
        if not os.path.isfile(source):
            raise SystemExit("\u043d\u0435\u0442 \u0444\u0430\u0439\u043b\u0430 \u0443\u0441\u0442\u0430\u043d\u043e\u0432\u0449\u0438\u043a\u0430: " + source)
        write_bytes(os.path.join(WORK, name), io.open(source, "rb").read())

    if os.path.exists(OUT):
        os.remove(OUT)
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for dirpath, dirs, names in os.walk(WORK):
            for n in names:
                if n.lower().endswith(".zip"):
                    continue
                path = os.path.join(dirpath, n)
                z.write(path, os.path.relpath(path, WORK).replace(os.sep, "/"))

    data = io.open(OUT, "rb").read()
    print("\u043f\u0430\u0442\u0447: %s" % OUT)
    print("\u0440\u0430\u0437\u043c\u0435\u0440: %.1f \u041c\u0411, \u0444\u0430\u0439\u043b\u043e\u0432 %d, md5 %s" %
          (len(data) / 1048576.0, len(zipfile.ZipFile(OUT).namelist()),
           hashlib.md5(data).hexdigest()))


if __name__ == "__main__":
    build()
