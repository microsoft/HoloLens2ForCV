"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
import tarfile

import numpy as np
import cv2

from hand_defs import HandJointIndex

# Depth values are saved inside a 16bit png with the following scaling factor
# This correponds to the scaling factor used by the TUM slam dataset:w
DEPTH_SCALING_FACTOR = 5000

folders_extensions = [('PV', 'bytes'),
                      ('Depth AHaT', '[0-9].pgm'),
                      ('Depth Long Throw', '[0-9].pgm'),
                      ('VLC LF', '[0-9].pgm'),
                      ('VLC RF', '[0-9].pgm'),
                      ('VLC LL', '[0-9].pgm'),
                      ('VLC RR', '[0-9].pgm')]


def extract_tar_file(tar_filename, output_path):
    tar = tarfile.open(tar_filename)
    tar.extractall(output_path)
    tar.close()


def load_lut(lut_filename):
    with open(lut_filename, mode='rb') as depth_file:
        lut = np.frombuffer(depth_file.read(), dtype="f")
        lut = np.reshape(lut, (-1, 3))
    return lut


def check_framerates(capture_path):
    HundredsOfNsToMilliseconds = 1e-4
    MillisecondsToSeconds = 1e-3

    def get_avg_delta(timestamps):
        deltas = [(timestamps[i] - timestamps[i-1]) for i in range(1, len(timestamps))]
        return np.mean(deltas)

    for (img_folder, img_ext) in folders_extensions:
        base_folder = capture_path / img_folder
        paths = base_folder.glob('*%s' % img_ext)
        timestamps = [int(path.stem) for path in paths]
        if len(timestamps):
            avg_delta = get_avg_delta(timestamps) * HundredsOfNsToMilliseconds
            print('Average {} delta: {:.3f}ms, fps: {:.3f}'.format(
                img_folder, avg_delta, 1/(avg_delta * MillisecondsToSeconds)))

    head_hat_stream_path = capture_path.glob('*eye.csv')
    try:
        head_hat_stream_path = next(head_hat_stream_path)
        timestamps = load_head_hand_eye_data(str(head_hat_stream_path))[0]
        hh_avg_delta = get_avg_delta(timestamps) * HundredsOfNsToMilliseconds
        print('Average hand/head delta: {:.3f}ms, fps: {:.3f}'.format(
            hh_avg_delta, 1/(hh_avg_delta * MillisecondsToSeconds)))
    except StopIteration:
        pass


def load_head_hand_eye_data(csv_path):
    joint_count = HandJointIndex.Count.value

    data = np.loadtxt(csv_path, delimiter=',')

    n_frames = len(data)
    timestamps = np.zeros(n_frames)
    head_transs = np.zeros((n_frames, 3))

    left_hand_transs = np.zeros((n_frames, joint_count, 3))
    left_hand_transs_available = np.ones(n_frames, dtype=bool)
    right_hand_transs = np.zeros((n_frames, joint_count, 3))
    right_hand_transs_available = np.ones(n_frames, dtype=bool)

    # origin (vector, homog) + direction (vector, homog) + distance (scalar)
    gaze_data = np.zeros((n_frames, 9))
    gaze_available = np.ones(n_frames, dtype=bool)

    for i_frame, frame in enumerate(data):
        timestamps[i_frame] = frame[0]
        # head
        head_transs[i_frame, :] = frame[1:17].reshape((4, 4))[:3, 3]
        # left hand
        left_hand_transs_available[i_frame] = (frame[17] == 1)
        left_start_id = 18
        for i_j in range(joint_count):
            j_start_id = left_start_id + 16 * i_j
            j_trans = frame[j_start_id:j_start_id + 16].reshape((4, 4))[:3, 3]
            left_hand_transs[i_frame, i_j, :] = j_trans
        # right hand
        right_hand_transs_available[i_frame] = (
            frame[left_start_id + joint_count * 4 * 4] == 1)
        right_start_id = left_start_id + joint_count * 4 * 4 + 1
        for i_j in range(joint_count):
            j_start_id = right_start_id + 16 * i_j
            j_trans = frame[j_start_id:j_start_id + 16].reshape((4, 4))[:3, 3]
            right_hand_transs[i_frame, i_j, :] = j_trans

        assert(j_start_id + 16 == 851)
        gaze_available[i_frame] = (frame[851] == 1)
        gaze_data[i_frame, :4] = frame[852:856]
        gaze_data[i_frame, 4:8] = frame[856:860]
        gaze_data[i_frame, 8] = frame[860]

    return (timestamps, head_transs, left_hand_transs, left_hand_transs_available,
            right_hand_transs, right_hand_transs_available, gaze_data, gaze_available)


def project_on_pv(points, pv_img, pv2world_transform, focal_length, principal_point):
    height, width, _ = pv_img.shape

    homog_points = np.hstack((points, np.ones(len(points)).reshape((-1, 1))))
    world2pv_transform = np.linalg.inv(pv2world_transform)
    points_pv = (world2pv_transform @ homog_points.T).T[:, :3]

    intrinsic_matrix = np.array([[focal_length[0], 0, principal_point[0]], [
        0, focal_length[1], principal_point[1]], [0, 0, 1]])
    rvec = np.zeros(3)
    tvec = np.zeros(3)
    xy, _ = cv2.projectPoints(points_pv, rvec, tvec, intrinsic_matrix, None)
    xy = np.squeeze(xy)
    xy[:, 0] = width - xy[:, 0]
    xy = np.around(xy).astype(int)

    rgb = np.zeros_like(points)
    width_check = np.logical_and(0 <= xy[:, 0], xy[:, 0] < width)
    height_check = np.logical_and(0 <= xy[:, 1], xy[:, 1] < height)
    valid_ids = np.where(np.logical_and(width_check, height_check))[0]

    z = points_pv[valid_ids, 2]
    xy = xy[valid_ids, :]

    depth_image = np.zeros((height, width))
    for i, p in enumerate(xy):
        depth_image[p[1], p[0]] = z[i]

    colors = pv_img[xy[:, 1], xy[:, 0], :]
    rgb[valid_ids, :] = colors[:, ::-1] / 255.

    return rgb, depth_image


def project_on_depth(points, rgb, intrinsic_matrix, width, height):
    rvec = np.zeros(3)
    tvec = np.zeros(3)
    xy, _ = cv2.projectPoints(points, rvec, tvec, intrinsic_matrix, None)
    xy = np.squeeze(xy)
    xy = np.around(xy).astype(int)

    width_check = np.logical_and(0 <= xy[:, 0], xy[:, 0] < width)
    height_check = np.logical_and(0 <= xy[:, 1], xy[:, 1] < height)
    valid_ids = np.where(np.logical_and(width_check, height_check))[0]
    xy = xy[valid_ids, :]

    z = points[valid_ids, 2]
    depth_image = np.zeros((height, width))
    image = np.zeros((height, width, 3))
    rgb = rgb[valid_ids, :]
    rgb = rgb[:, ::-1]
    for i, p in enumerate(xy):
        depth_image[p[1], p[0]] = z[i]
        image[p[1], p[0]] = rgb[i]

    image = image * 255.

    return image, depth_image
