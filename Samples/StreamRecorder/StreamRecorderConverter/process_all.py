"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
import argparse
from pathlib import Path
from project_hand_eye_to_pv import project_hand_eye_to_pv
from utils import check_framerates, extract_tar_file
from save_pclouds import save_pclouds
from convert_images import convert_images


def process_all(w_path, project_hand_eye=False):
    # Extract all tar
    for tar_fname in w_path.glob("*.tar"):
        print(f"Extracting {tar_fname}")
        tar_output = ''
        tar_output = w_path / Path(tar_fname.stem)
        tar_output.mkdir(exist_ok=True)
        extract_tar_file(tar_fname, tar_output)

    # Process PV if recorded
    if (w_path / "PV.tar").exists():
        # Convert images
        convert_images(w_path)

        # Project
        if project_hand_eye:
            project_hand_eye_to_pv(w_path)
# Process depth if recorded
    for sensor_name in ["Depth Long Throw", "Depth AHaT"]:
        if (w_path / "{}.tar".format(sensor_name)).exists():
            # Save point clouds
            save_pclouds(w_path, sensor_name)
    print("")
    check_framerates(w_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Process recorded data.')
    parser.add_argument("--recording_path", required=True,
                        help="Path to recording folder")
    parser.add_argument("--project_hand_eye",
                        required=False,
                        action='store_true',
                        help="Project hand joints (and eye gaze, if recorded) to rgb images")

    args = parser.parse_args()

    w_path = Path(args.recording_path)

    process_all(w_path, args.project_hand_eye)
