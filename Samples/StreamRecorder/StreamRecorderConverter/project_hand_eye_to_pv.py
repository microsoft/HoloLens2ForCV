"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
import cv2
import argparse
import numpy as np
from pathlib import Path
import ast

from utils import load_head_hand_eye_data


def process_timestamps(path):
    with open(path) as f:
        lines = f.readlines()
    print('Num timestamps:', len(lines))
    return np.array([int(elem) for elem in lines if len(elem)])


def load_pv_data(csv_path):
    with open(csv_path) as f:
        lines = f.readlines()

    # The first line contains info about the intrinsics.
    # The following lines (one per frame) contain timestamp, focal length and transform PVtoWorld
    n_frames = len(lines) - 1
    frame_timestamps = np.zeros(n_frames, dtype=np.longlong)
    focal_lengths = np.zeros((n_frames, 2))
    pv2world_transforms = np.zeros((n_frames, 4, 4))

    intrinsics_ox, intrinsics_oy, \
        intrinsics_width, intrinsics_height = ast.literal_eval(lines[0])

    for i_frame, frame in enumerate(lines[1:]):
        # Row format is
        # timestamp, focal length (2), transform PVtoWorld (4x4)
        frame = frame.split(',')
        frame_timestamps[i_frame] = int(frame[0])
        focal_lengths[i_frame, 0] = float(frame[1])
        focal_lengths[i_frame, 1] = float(frame[2])
        pv2world_transforms[i_frame] = np.array(frame[3:20]).astype(float).reshape((4, 4))

    return (frame_timestamps, focal_lengths, pv2world_transforms,
            intrinsics_ox, intrinsics_oy, intrinsics_width, intrinsics_height)


def match_timestamp(target, all_timestamps):
    return np.argmin([abs(x - target) for x in all_timestamps])


def get_eye_gaze_point(gaze_data):
    origin_homog = gaze_data[:4]
    direction_homog = gaze_data[4:8]
    direction_homog = direction_homog / np.linalg.norm(direction_homog)
    # if no distance was recorded, set 1m by default
    dist = gaze_data[8] if gaze_data[8] > 0.0 else 1.0
    point = origin_homog + direction_homog * dist

    return point[:3]


def project_hand_eye_to_pv(folder):
    print("")
    head_hat_stream_path = list(folder.glob('*_eye.csv'))[0]
    pv_info_path = list(folder.glob('*pv.txt'))[0]
    pv_paths = sorted(list((folder / 'PV').glob('*png')))
    assert(len(pv_paths))

    # load head, hand, eye data
    (timestamps, _,
     left_hand_transs, left_hand_transs_available,
     right_hand_transs, right_hand_transs_available,
     gaze_data, gaze_available) = load_head_hand_eye_data(head_hat_stream_path)

    eye_str = " and eye gaze" if np.any(gaze_available) else ""
    print("Projecting hand joints{} to PV".format(eye_str))

    # load pv info
    (frame_timestamps, focal_lengths, pv2world_transforms,
     ox, oy, width, height) = load_pv_data(pv_info_path)

    principal_point = np.array([ox, oy])

    n_frames = len(pv_paths)
    output_folder = folder / 'eye_hands'
    output_folder.mkdir(exist_ok=True)
    for pv_id in range(n_frames):
        print(".", end="", flush=True)
        pv_path = pv_paths[pv_id]
        sample_timestamp = int(str(pv_path.name).replace('.png', ''))

        hand_ts = match_timestamp(sample_timestamp, timestamps)
        # print('Frame-hand delta: {:.3f}ms'.format((sample_timestamp - timestamps[hand_ts]) * 1e-4))

        img = cv2.imread(str(pv_path))
        # pinhole
        K = np.array([[focal_lengths[pv_id][0], 0, principal_point[0]],
                      [0, focal_lengths[pv_id][1], principal_point[1]],
                      [0, 0, 1]])
        try:
            Rt = np.linalg.inv(pv2world_transforms[pv_id])
        except np.linalg.LinAlgError:
            print('No pv2world transform')
            continue
        rvec, _ = cv2.Rodrigues(Rt[:3, :3])
        tvec = Rt[:3, 3]

        colors = [(0, 0, 255), (0, 255, 0), (255, 0, 0)]
        hands = [(left_hand_transs, left_hand_transs_available),
                 (right_hand_transs, right_hand_transs_available)]
        for hand_id, hand in enumerate(hands):
            transs, avail = hand
            if avail[hand_ts]:
                for joint in transs[hand_ts]:
                    hand_tr = joint.reshape((1, 3))
                    xy, _ = cv2.projectPoints(hand_tr, rvec, tvec, K, None)
                    ixy = (int(xy[0][0][0]), int(xy[0][0][1]))
                    ixy = (width - ixy[0], ixy[1])
                    img = cv2.circle(img, ixy, radius=3, color=colors[hand_id])

        if gaze_available[hand_ts]:
            point = get_eye_gaze_point(gaze_data[hand_ts])
            xy, _ = cv2.projectPoints(point.reshape((1, 3)), rvec, tvec, K, None)
            ixy = (int(xy[0][0][0]), int(xy[0][0][1]))
            ixy = (width - ixy[0], ixy[1])
            img = cv2.circle(img, ixy, radius=3, color=colors[2])

        cv2.imwrite(str(output_folder / 'hands') + 'proj{}.png'.format(str(pv_id).zfill(4)), img)


if __name__ == "__main__":
    # pass the path to folder being processed
    parser = argparse.ArgumentParser(description='Process recorded data.')
    parser.add_argument("--recording_path", required=True,
                        help="Path to recording folder")

    args = parser.parse_args()
    project_hand_eye_to_pv(Path(args.recording_path))
