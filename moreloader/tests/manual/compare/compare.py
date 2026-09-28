# Runs moresampler natively on Windows and through moreloader in WSL with the same arguments, and
# compares every file that the two runs produce byte by byte.
#
#     python compare.py [--voice <voice bank>] [--wav name.wav ...] [--case frq|render|wavtool ...]
#
# Run on Windows from any directory. The Windows side works in work/compare of the repository:
#   bin/            moresampler.exe from work/moresampler and a moreconfig.txt for testing
#   source/         the input voice bank: the selected wav files and their oto.ini lines
#   windows/        a fresh copy of source/, into which the runs write
# The Linux side works in ~/moreloader-compare of WSL, with its own bin/ and linux/, because files
# under /mnt are accessed through a slow network file system.
#
# Both sides of a step run in parallel. Progress with the duration of each run is written to the
# standard output and to work/compare/progress.log.
#
# The loader must have been built in WSL into build/out/bin/moreloader. The voice bank and the
# executable are copied, the originals are only read.
#
# Timestamps are excluded from the comparison of desc.mrq, because each entry records the time
# at which it was written.

import argparse
import concurrent.futures
import os
import shutil
import struct
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
WORK = os.path.join(ROOT, 'work', 'compare')
PROGRESS = os.path.join(WORK, 'progress.log')
MORESAMPLER_DIR = os.path.join(ROOT, 'work', 'moresampler')
LOADER = '/mnt/e/GitHub/moreloader/build/out/bin/moreloader'
DISTRO = 'Ubuntu-24.04'

DEFAULT_VOICE = (r'C:\Users\user\Downloads\歌声合成软件 UTAU v0.4.18 完整汉化版【修复4.5】'
                 r'\歌声合成软件 UTAU v0.4.18 完整汉化版【修复4.5】\UTAU 主程序\UTAU\voice\Extra-Jinkela')

CONFIG = '''# Configuration for the comparison of moreloader with Windows
output-sampling-rate 44100
output-bit-depth 16
resampler-compatibility on
synthesis-utau-style-normalization full
synthesis-loudness-preservation off
synthesis-duration-extension-method auto
dump-log-file off
multithread-synthesis off
auto-update-llsm-mrq on
analysis-anti-distortion off
analysis-noise-reduction off
analysis-biased-f0-estimation off
analysis-suppress-subharmonics off
analysis-f0-range-from-path on
analysis-f0-min 70.0
analysis-f0-max 800.0
load-frq off
'''

# Home directory in WSL, determined at start.
linuxHome = None


def log(message):
    line = time.strftime('%H:%M:%S ') + message
    print(line, flush=True)
    with open(PROGRESS, 'a', encoding='utf-8') as f:
        f.write(line + '\n')


def wsl(command):
    """Runs a shell command in WSL and returns its standard output."""
    result = subprocess.run(['wsl', '-d', DISTRO, '-e', 'bash', '-c', command],
                            capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.decode('utf-8', 'replace'))
    return result.stdout.decode('utf-8', 'replace').strip()


def wsl_path(path):
    """Returns the WSL path of a Windows path."""
    drive, rest = os.path.splitdrive(os.path.abspath(path))
    return '/mnt/' + drive[0].lower() + rest.replace('\\', '/')


def linux_work():
    return linuxHome + '/moreloader-compare'


def linux_unc(path):
    """Returns the Windows path of a path in the file system of WSL."""
    return '\\\\wsl.localhost\\' + DISTRO + path.replace('/', '\\')


def prepare(voice, wavs):
    for name in ('bin', 'source', 'windows'):
        if os.path.isdir(os.path.join(WORK, name)):
            shutil.rmtree(os.path.join(WORK, name))
    os.makedirs(os.path.join(WORK, 'bin'))
    shutil.copy2(os.path.join(MORESAMPLER_DIR, 'moresampler.exe'), os.path.join(WORK, 'bin'))
    with open(os.path.join(WORK, 'bin', 'moreconfig.txt'), 'w', newline='\r\n') as f:
        f.write(CONFIG)

    source = os.path.join(WORK, 'source')
    os.makedirs(source)
    for wav in wavs:
        shutil.copy2(os.path.join(voice, wav), source)
    with open(os.path.join(voice, 'oto.ini'), 'rb') as f:
        lines = [line for line in f.read().split(b'\r\n')
                 if line.split(b'=')[0].decode('ascii', 'replace') in wavs]
    with open(os.path.join(source, 'oto.ini'), 'wb') as f:
        f.write(b'\r\n'.join(lines) + b'\r\n')

    shutil.copytree(source, os.path.join(WORK, 'windows', 'voice'))
    os.makedirs(os.path.join(WORK, 'windows', 'out'))

    target = linux_work()
    wsl(f"rm -rf '{target}' && mkdir -p '{target}/bin' '{target}/linux/out' && "
        f"cp '{wsl_path(os.path.join(WORK, 'bin'))}'/* '{target}/bin/' && "
        f"cp -r '{wsl_path(source)}' '{target}/linux/voice'")


def run(side, arguments):
    """Runs moresampler on one side. Arguments are templates in which {voice} and {out} are
    replaced by the directories of that side, and {sep} by its path separator."""
    started = time.monotonic()
    if side == 'windows':
        voice = os.path.join(WORK, side, 'voice')
        out = os.path.join(WORK, side, 'out')
        exe = os.path.join(WORK, 'bin', 'moresampler.exe')
        args = [a.format(voice=voice, out=out, sep='\\') for a in arguments]
        command = [exe] + args
        cwd = out
    else:
        voice = linux_work() + '/linux/voice'
        out = linux_work() + '/linux/out'
        exe = linux_work() + '/bin/moresampler.exe'
        args = [a.format(voice=voice, out=out, sep='/') for a in arguments]
        command = ['wsl', '-d', DISTRO, '--cd', out, '-e', LOADER, exe] + args
        cwd = None
    result = subprocess.run(command, cwd=cwd, stdin=subprocess.DEVNULL, capture_output=True)
    return (result.returncode, result.stdout.decode('utf-8', 'replace'),
            result.stderr.decode('utf-8', 'replace'), time.monotonic() - started)


def mrq_without_timestamps(data):
    """Returns desc.mrq with the timestamp of every entry set to zero."""
    data = bytearray(data)
    if data[:4] != b'mrq ':
        return bytes(data)
    count = struct.unpack_from('<i', data, 8)[0]
    position = 12
    for _ in range(count):
        name_length = struct.unpack_from('<i', data, position)[0]
        position += 4 + 2 * name_length
        size = struct.unpack_from('<i', data, position)[0]
        position += 4
        entry = position
        f0_count = struct.unpack_from('<i', data, entry)[0]
        timestamp = entry + 12 + 4 * f0_count
        if timestamp + 4 <= len(data):
            data[timestamp:timestamp + 4] = b'\0\0\0\0'
        position = entry + size
    return bytes(data)


def compare_trees():
    left = os.path.join(WORK, 'windows')
    right = linux_unc(linux_work() + '/linux')
    names = set()
    for base in (left, right):
        for directory, _, files in os.walk(base):
            for name in files:
                names.add(os.path.relpath(os.path.join(directory, name), base))
    ok = True
    for name in sorted(names):
        a = os.path.join(left, name)
        b = os.path.join(right, name)
        if not os.path.exists(a) or not os.path.exists(b):
            log(f'  {name}: only on {"windows" if os.path.exists(a) else "linux"}')
            ok = False
            continue
        x = open(a, 'rb').read()
        y = open(b, 'rb').read()
        # The index files of the wavtool mode record absolute paths, which differ between the
        # sides by their root. Both roots are replaced by one placeholder, in UTF-16 and in the
        # narrow encoding.
        windows_root = os.path.join(WORK, 'windows')
        linux_root = 'Z:' + (linux_work() + '/linux').replace('/', '\\')
        for root, data in ((windows_root, 'x'), (linux_root, 'y')):
            for encoding in ('utf-16-le', 'utf-8'):
                placeholder = '<ROOT>'.encode(encoding)
                if data == 'x':
                    x = x.replace(root.encode(encoding), placeholder)
                else:
                    y = y.replace(root.encode(encoding), placeholder)
        if os.path.basename(name).lower() == 'desc.mrq':
            x = mrq_without_timestamps(x)
            y = mrq_without_timestamps(y)
        if x == y:
            log(f'  {name}: identical ({len(x)} bytes)')
            continue
        ok = False
        differing = sum(1 for i in range(min(len(x), len(y))) if x[i] != y[i])
        first = next((i for i in range(min(len(x), len(y))) if x[i] != y[i]), min(len(x), len(y)))
        log(f'  {name}: DIFFERENT, sizes {len(x)} and {len(y)}, {differing} differing bytes, '
            f'first at {first}')
    return ok


def main():
    global linuxHome
    parser = argparse.ArgumentParser()
    parser.add_argument('--voice', default=DEFAULT_VOICE)
    parser.add_argument('--wav', action='append', default=None)
    parser.add_argument('--case', action='append', default=None)
    options = parser.parse_args()
    wavs = options.wav or ['ae.wav', 'baf.wav', 'bam.wav']
    cases = options.case or ['frq', 'render', 'wavtool']

    os.makedirs(WORK, exist_ok=True)
    if os.path.exists(PROGRESS):
        os.remove(PROGRESS)
    linuxHome = wsl('echo $HOME')
    log(f'preparing {WORK} and {linux_work()}')
    prepare(options.voice, wavs)

    steps = []
    if 'frq' in cases:
        # The command of frqeditor for generating a frequency table.
        for wav in wavs:
            steps.append(('frq ' + wav, ['{voice}{sep}' + wav, 'nul', '100', '100', 'GN', '0',
                                         '50']))
    if 'render' in cases or 'wavtool' in cases:
        # Resampler mode with the 13 arguments of UTAU.
        for wav in wavs[:2]:
            stem = wav[:-4]
            steps.append(('render ' + wav, ['{voice}{sep}' + wav, '{out}{sep}' + stem + '_out.wav',
                                            'C4', '100', '', '0', '500', '0', '0', '100', '0',
                                            '!120', 'AA#5#']))
    if 'wavtool' in cases:
        # Two notes concatenated in wavtool mode with the arguments of UTAU.
        stems = [w[:-4] for w in wavs[:2]]
        steps.append(('wavtool 1', ['{out}{sep}mix.wav', '{out}{sep}' + stems[0] + '_out.wav', '0',
                                    '480@120+0', '0', '5', '35', '0', '100', '100', '0', '10']))
        steps.append(('wavtool 2', ['{out}{sep}mix.wav', '{out}{sep}' + stems[1] + '_out.wav', '0',
                                    '480@120+0', '5', '5', '35', '0', '100', '100', '0', '10']))

    total = time.monotonic()
    for number, (title, arguments) in enumerate(steps, 1):
        log(f'[{number}/{len(steps)}] {title}: running on both sides')
        with concurrent.futures.ThreadPoolExecutor(2) as pool:
            futures = {side: pool.submit(run, side, arguments) for side in ('windows', 'linux')}
            for side in ('windows', 'linux'):
                code, out, err, seconds = futures[side].result()
                lines = (out + err).strip().splitlines()
                last = lines[-1] if lines else ''
                log(f'  {side}: exit {code} in {seconds:.1f} s; last line: {last[:200]}')

    log(f'comparison after {time.monotonic() - total:.1f} s')
    ok = compare_trees()
    log('RESULT: ' + ('all files identical' if ok else 'differences found'))
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
