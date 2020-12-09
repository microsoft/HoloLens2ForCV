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

import numpy as np
import open3d as o3d

from utils import DEPTH_SCALING_FACTOR


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='TSDF-Integration with open3d')
    parser.add_argument("--pinhole_path",
                        required=True,
                        help="Path to folder inside recording containing pinhole projected images "
                        "recordings")

    parser.add_argument("--voxel_size",
                        required=False,
                        default=0.04,
                        type=float,
                        help="Voxel size to use for tsdf integration."
                        "Bigger values results in denser but slower reconstructions.")

    args = parser.parse_args()
    pinhole_path = Path(args.pinhole_path)
    # WARNING: in read_pinhole_camera_trajectory extrinsic gets inverted!
    trajectory = o3d.io.read_pinhole_camera_trajectory(
        str(pinhole_path / 'odometry.log'))

    # Take care of open3d api change from 0.11
    o3d_integration = None
    o3d_version = float(o3d.__version__[:o3d.__version__.rfind('.')])
    if o3d_version < 0.11:
        o3d_integration = o3d.integration
    else:
        o3d_integration = o3d.pipelines.integration
        
    volume = o3d_integration.ScalableTSDFVolume(
        voxel_length=args.voxel_size,
        sdf_trunc=args.voxel_size*3,  # truncation value is set at 3x voxel size
        color_type=o3d_integration.TSDFVolumeColorType.RGB8)
    #   color_type=o3d.integration.TSDFVolumeColorType.NoColor)

    intrinsic_path = pinhole_path / "calibration.txt"
    rgb_file_list = pinhole_path / "rgb.txt"
    depth_file_list = pinhole_path / "depth.txt"
    print(f"Integrating {len(trajectory.parameters)} images")

    intrinsic_np = np.loadtxt(str(intrinsic_path))

    with open(str(rgb_file_list)) as rf, open(str(depth_file_list)) as df:
        i = 0
        while True:
            rgb_line = rf.readline()
            depth_line = df.readline()
            if not rgb_line or not depth_line:
                break
            rgb_path = str(pinhole_path / rgb_line.split()[1])
            depth_path = str(pinhole_path / depth_line.split()[1])

            print(".", end="", flush=True)
            color = o3d.io.read_image(rgb_path)
            depth = o3d.io.read_image(depth_path)
            depth_np = np.asarray(depth)
            intrinsic = o3d.camera.PinholeCameraIntrinsic(
                depth_np.shape[1], depth_np.shape[0],
                intrinsic_np[0], intrinsic_np[1],
                intrinsic_np[2], intrinsic_np[3])
            rgbd = o3d.geometry.RGBDImage.create_from_color_and_depth(
                color, depth,
                depth_scale=DEPTH_SCALING_FACTOR,
                depth_trunc=7.8, convert_rgb_to_intensity=False)
            volume.integrate(
                rgbd,
                intrinsic,
                trajectory.parameters[i].extrinsic)
            i = i + 1
    print("\n")

    mesh = volume.extract_triangle_mesh()
    mesh.compute_vertex_normals()
    mesh_path = str(pinhole_path / 'tsdf-mesh.ply')
    print(f"Saving mesh to {mesh_path}")
    o3d.io.write_triangle_mesh(mesh_path, mesh)

    pc = volume.extract_point_cloud()
    pc.estimate_normals()
    pc_path = str(pinhole_path / 'tsdf-pc.ply')
    print(f"Saving point cloud to {pc_path}")
    o3d.io.write_point_cloud(pc_path, pc)

    o3d.visualization.draw_geometries([pc])
