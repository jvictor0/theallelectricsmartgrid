"""Stereo export shared by the USB sync tool and LAN receiver."""

import subprocess
from pathlib import Path

from extract_recording import extract
from sgrec import x_magic


def extract_stereo_recording(local_path: Path, output_path: Path | None = None) -> int:
    local_path = Path(local_path)
    with local_path.open('rb') as source:
        magic = source.read(12)
    if magic[:8] == x_magic:
        result = extract(local_path, output_path=output_path, master='stereo', overwrite=True)
        print(f'  Extracted {result.m_frames} frames: {result.m_output.name}')
        if not result.m_complete:
            print(f'  Incomplete recording: {result.m_error}')
            return 1
        return 0
    if magic[:4] not in (b'RIFF', b'RF64') or magic[8:12] != b'WAVE':
        raise ValueError(f'{local_path} is not a SmartGrid or RIFF/RF64 recording')

    channel_count = int(subprocess.check_output(['soxi', '-c', str(local_path)], text=True).strip())
    if channel_count < 2:
        raise ValueError(f'{local_path} has only {channel_count} channel(s)')
    stereo_path = output_path or local_path.with_stem(local_path.stem + '_stereo')
    command = ['sox', str(local_path)]
    if output_path is not None:
        command.extend(['-t', 'wav'])
    command.extend([str(stereo_path), 'remix', str(channel_count - 1), str(channel_count)])
    subprocess.run(command, check=True)
    return 0
