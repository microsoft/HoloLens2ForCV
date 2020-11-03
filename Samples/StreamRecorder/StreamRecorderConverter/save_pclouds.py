"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
import argparse
import multiprocessing
from pathlib import Path

import numpy as np
import cv2
import open3d as o3d

from project_hand_eye_to_pv import load_pv_data, match_timestamp
from utils import extract_tar_file, load_lut, DEPTH_SCALING_FACTOR, project_on_depth, project_on_pv


def save_output_txt_files(folder, shared_dict):
    """Save output txt files saved inside shared_dict
    depth.txt -> list of depth images
    rgb.txt -> list of rgb images
    trajectory.xyz -> list of camera centers
    odometry.log -> odometry file in open3d format

    Args:
        folder ([Path]): Output folder
        shared_dict ([dictionary]): Dictionary containing depth image filename, rgb image filename, camera position, pose
    """
    depth_path = Path(folder / 'depth.txt')
    rgb_path = Path(folder / 'rgb.txt')
    traj_path = Path(folder / 'trajectory.xyz')
    odo_path = Path(folder / 'odometry.log')

    with open(str(depth_path), "w") as df:
        with open(str(rgb_path), "w") as rf:
            with open(str(traj_path), "w") as tf:
                with open(str(odo_path), "w") as of:
                    i = 0
                    for timestamp in shared_dict:
                        df.write(f"{timestamp} {shared_dict[timestamp][0]}\n")
                        rf.write(f"{timestamp} {shared_dict[timestamp][1]}\n")
                        camera_string = ' '.join(map(str, shared_dict[timestamp][2]))
                        tf.write(f"{camera_string}\n")
                        pose = shared_dict[timestamp][3]
                        of.write(f"{i} {i} {i}\n")
                        of.write(f"{pose[0,0]} {pose[0,1]} {pose[0,2]} {pose[0,3]}\n")
                        of.write(f"{pose[1,0]} {pose[1,1]} {pose[1,2]} {pose[1,3]}\n")
                        of.write(f"{pose[2,0]} {pose[2,1]} {pose[2,2]} {pose[2,3]}\n")
                        of.write(f"{pose[3,0]} {pose[3,1]} {pose[3,2]} {pose[3,3]}\n")
                        i = i + 1


def save_single_pcloud(shared_dict,
                       path,
                       folder,
                       pinhole_folder,
                       save_in_cam_space,
                       lut,
                       has_pv,
                       focal_lengths,
                       principal_point,
                       rig2world_transforms,
                       rig2cam,
                       pv_timestamps,
                       pv2world_transforms,
                       discard_no_rgb,
                       clamp_min,
                       clamp_max,
                       depth_path_suffix,
                       disable_project_pinhole
                       ):
    suffix = '_cam' if save_in_cam_space else ''
    output_path = str(path)[:-4] + f'{suffix}.ply'


#    if Path(output_path).exists():
#        print(output_path + ' is already exists, skip generating this pclouds')
#        return


    print(".", end="", flush=True)

    # extract the timestamp for this frame
    timestamp = extract_timestamp(path.name.replace(depth_path_suffix, ''))
    # load depth img
    img = cv2.imread(str(path), -1)
    height, width = img.shape
    assert len(lut) == width * height

    # Clamp values if requested
    if clamp_min > 0 and clamp_max > 0:
        # Convert crop values to mm
        clamp_min = clamp_min * 1000.
        clamp_max = clamp_max * 1000.
        # Clamp depth values
        img[img < clamp_min] = 0
        img[img > clamp_max] = 0

    # Get xyz points in camera space
    points = get_points_in_cam_space(img, lut)
    if save_in_cam_space:
        save_ply(output_path, points, rgb=None)
        # print('Saved %s' % output_path)
    else:
        if rig2world_transforms and (timestamp in rig2world_transforms):
            # if we have the transform from rig to world for this frame,
            # then put the point clouds in world space
            rig2world = rig2world_transforms[timestamp]
            # print('Transform found for timestamp %s' % timestamp)
            xyz, cam2world_transform = cam2world(points, rig2cam, rig2world)

            rgb = None
            if has_pv:
                # if we have pv, get vertex colors
                # get the pv frame which is closest in time
                target_id = match_timestamp(timestamp, pv_timestamps)
                pv_ts = pv_timestamps[target_id]
                rgb_path = str(folder / 'PV' / f'{pv_ts}.png')
                assert Path(rgb_path).exists()
                pv_img = cv2.imread(rgb_path)

                # Project from depth to pv going via world space
                rgb, depth = project_on_pv(
                    xyz, pv_img, pv2world_transforms[target_id], 
                    focal_lengths[target_id], principal_point)

                # Project depth on virtual pinhole camera and save corresponding
                # rgb image inside <workspace>/pinhole_projection folder
                if not disable_project_pinhole:
                    # Create virtual pinhole camera
                    scale = 1
                    width = 320 * scale
                    height = 288 * scale
                    focal_length = 200 * scale
                    intrinsic_matrix = np.array([[focal_length, 0, width / 2.],
                                                 [0, focal_length, height / 2.],
                                                 [0, 0, 1.]])
                    rgb_proj, depth = project_on_depth(
                        points, rgb, intrinsic_matrix, width, height)

                    # Save depth image
                    depth_proj_folder = pinhole_folder / 'depth' / f'{pv_ts}.png'
                    depth_proj_path = str(depth_proj_folder)[:-4] + f'{suffix}_proj.png'
                    depth = (depth * DEPTH_SCALING_FACTOR).astype(np.uint16)
                    cv2.imwrite(depth_proj_path, (depth).astype(np.uint16))

                    # Save rgb image
                    rgb_proj_folder = pinhole_folder / 'rgb' / f'{pv_ts}.png'
                    rgb_proj_path = str(rgb_proj_folder)[:-4] + f'{suffix}_proj.png'
                    cv2.imwrite(rgb_proj_path, rgb_proj)

                    # Save virtual pinhole information inside calibration.txt
                    intrinsic_path = pinhole_folder / Path('calibration.txt')
                    intrinsic_list = [intrinsic_matrix[0, 0], intrinsic_matrix[1, 1],
                                      intrinsic_matrix[0, 2], intrinsic_matrix[1, 2]]
                    with open(str(intrinsic_path), "w") as p:
                        p.write(f"{intrinsic_list[0]} \
                                {intrinsic_list[1]} \
                                {intrinsic_list[2]} \
                                {intrinsic_list[3]} \n")

                    # Create rgb and depth paths
                    rgb_parts = Path(rgb_proj_path).parts[2:]
                    rgb_tmp = Path(rgb_parts[-2]) / Path(rgb_parts[-1])
                    depth_parts = Path(depth_proj_path).parts[2:]
                    depth_tmp = Path(depth_parts[-2]) / Path(depth_parts[-1])

                    # Compute camera center
                    camera_center = cam2world_transform @ np.array([0, 0, 0, 1])

                    # Save depth, rgb, camera center, extrinsics inside shared dictionary
                    shared_dict[path.stem] = [depth_tmp, rgb_tmp,
                                              camera_center[:3], cam2world_transform]

            if discard_no_rgb:
                colored_points = rgb[:, 0] > 0
                xyz = xyz[colored_points]
                rgb = rgb[colored_points]
            save_ply(output_path, xyz, rgb, cam2world_transform)
            # print('Saved %s' % output_path)
        else:
            print('Transform not found for timestamp %s' % timestamp)


def save_ply(output_path, points, rgb=None, cam2world_transform=None):
    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(points)
    if rgb is not None:
        pcd.colors = o3d.utility.Vector3dVector(rgb)
    pcd.estimate_normals()
    if cam2world_transform is not None:
        # Camera center
        camera_center = (cam2world_transform) @ np.array([0, 0, 0, 1])
        o3d.geometry.PointCloud.orient_normals_towards_camera_location(pcd, camera_center[:3])

    o3d.io.write_point_cloud(output_path, pcd)


def load_extrinsics(extrinsics_path):
    assert Path(extrinsics_path).exists()
    mtx = np.loadtxt(str(extrinsics_path), delimiter=',').reshape((4, 4))
    return mtx


def get_points_in_cam_space(img, lut):
    img = np.tile(img.flatten().reshape((-1, 1)), (1, 3))
    points = img * lut
    remove_ids = np.where(np.sum(points, axis=1) < 1e-6)[0]
    points = np.delete(points, remove_ids, axis=0)
    points /= 1000.
    return points


def cam2world(points, rig2cam, rig2world):
    homog_points = np.hstack((points, np.ones((points.shape[0], 1))))
    cam2world_transform = rig2world @ np.linalg.inv(rig2cam)
    world_points = cam2world_transform @ homog_points.T
    return world_points.T[:, :3], cam2world_transform


def extract_timestamp(path):
    return float(path.split('.')[0])


def load_rig2world_transforms(path):
    transforms = {}
    data = np.loadtxt(str(path), delimiter=',')
    for value in data:
        timestamp = value[0]
        transform = value[1:].reshape((4, 4))
        transforms[timestamp] = transform
    return transforms


def save_pclouds(folder,
                 sensor_name,
                 save_in_cam_space=False,
                 discard_no_rgb=False,
                 clamp_min=0.,
                 clamp_max=0.,
                 depth_path_suffix='',
                 disable_project_pinhole=False
                 ):
    print("")
    print("Saving point clouds")

    calib = r'{}_lut.bin'.format(sensor_name)
    extrinsics = r'{}_extrinsics.txt'.format(sensor_name)
    rig2world = r'{}_rig2world.txt'.format(sensor_name)
    calib_path = folder / calib
    rig2campath = folder / extrinsics
    rig2world_path = folder / rig2world if not save_in_cam_space else ''

    # check if we have pv
    has_pv = False
    try:
        if __name__ == '__main__':
            extract_tar_file(str(folder / 'PV.tar'), folder / 'PV')
    except FileNotFoundError:
        pass

    pv_info_path = sorted(folder.glob(r'*pv.txt'))
    has_pv = len(list(pv_info_path)) > 0
    if has_pv:
        (pv_timestamps, focal_lengths, pv2world_transforms, ox,
         oy, _, _) = load_pv_data(list(pv_info_path)[0])
        principal_point = np.array([ox, oy])
    else:
        pv_timestamps = focal_lengths = pv2world_transforms = ox = oy = principal_point = None

    # lookup table to extract xyz from depth
    lut = load_lut(calib_path)

    # from camera to rig space transformation (fixed)
    rig2cam = load_extrinsics(rig2campath)

    # from rig to world transformations (one per frame)
    rig2world_transforms = load_rig2world_transforms(
        rig2world_path) if rig2world_path != '' and Path(rig2world_path).exists() else None
    depth_path = Path(folder / sensor_name)
    depth_path.mkdir(exist_ok=True)

    pinhole_folder = None
    if not disable_project_pinhole and has_pv:
        # Create folders for pinhole projection
        pinhole_folder = folder / 'pinhole_projection'
        pinhole_folder.mkdir(exist_ok=True)

        pinhole_folder_rgb = pinhole_folder / 'rgb'
        pinhole_folder_rgb.mkdir(exist_ok=True)

        pinhole_folder_depth = pinhole_folder / 'depth'
        pinhole_folder_depth.mkdir(exist_ok=True)

    # Extract tar only when calling the script directly
    if __name__ == '__main__':
        extract_tar_file(str(folder / '{}.tar'.format(sensor_name)), str(depth_path))

    # Depth path suffix used for now only if we load masked AHAT
    depth_paths = sorted(depth_path.glob('*[0-9]{}.pgm'.format(depth_path_suffix)))
    assert len(list(depth_paths)) > 0 

    # Create shared dictionary to save odometry and file list
    manager = multiprocessing.Manager()
    shared_dict = manager.dict()

    multiprocess_pool = multiprocessing.Pool(multiprocessing.cpu_count())
    for path in depth_paths:
        multiprocess_pool.apply_async(
            save_single_pcloud(shared_dict,
                               path,
                               folder,
                               pinhole_folder,
                               save_in_cam_space,
                               lut,
                               has_pv,
                               focal_lengths,
                               principal_point,
                               rig2world_transforms,
                               rig2cam,
                               pv_timestamps,
                               pv2world_transforms,
                               discard_no_rgb,
                               clamp_min,
                               clamp_max,
                               depth_path_suffix,
                               disable_project_pinhole
                               )
        )
    multiprocess_pool.close()
    multiprocess_pool.join()

    if not disable_project_pinhole and has_pv:
        save_output_txt_files(pinhole_folder, shared_dict)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Save pcloud.')
    parser.add_argument("--recording_path",
                        required=True,
                        help="Path to recording folder")
    parser.add_argument("--cam_space",
                        required=False,
                        action='store_true',
                        help="Save in camera space (points will not be colored)")
    parser.add_argument("--disable_project_pinhole",
                        required=False,
                        default=False,
                        action='store_true',
                        help="Do not undistort depth and project to pinhole camera, \
                        results in faster conversion to pointcloud")
    parser.add_argument("--discard_no_rgb",
                        required=False,
                        action='store_true',
                        help="Get rid of 3d points not visible from rgb camera")
    parser.add_argument("--clamp_min",
                        required=False,
                        type=float,
                        default=0.,
                        help="Remove depth values less than clamp_min, unused when 0"
                        "recordings")
    parser.add_argument("--clamp_max",
                        required=False,
                        type=float,
                        default=0.,
                        help="Remove depth values greater than clamp_max, unused when 0"
                        "recordings")
    parser.add_argument("--depth_path_suffix",
                        default="",
                        choices=["", "_masked"],
                        help="Specify the suffix for depth img filenames, in order"
                             "to work on postprocessed ones (e.g. masked AHAT)")

    args = parser.parse_args()
    for sensor_name in ["Depth Long Throw", "Depth AHaT"]:
        if (Path(args.recording_path) / f"{sensor_name}.tar").exists():
            save_pclouds(Path(args.recording_path),
                         sensor_name,
                         args.cam_space,
                         args.discard_no_rgb,
                         args.clamp_min,
                         args.clamp_max,
                         args.depth_path_suffix,
                         args.disable_project_pinhole)
