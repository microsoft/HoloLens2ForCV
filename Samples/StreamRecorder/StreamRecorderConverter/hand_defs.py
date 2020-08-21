"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
from enum import Enum
import numpy as np


class HandJointIndex(Enum):
    Palm = 0
    Wrist = 1
    ThumbMetacarpal = 2
    ThumbProximal = 3
    ThumbDistal = 4
    ThumbTip = 5
    IndexMetacarpal = 6
    IndexProximal = 7
    IndexIntermediate = 8
    IndexDistal = 9
    IndexTip = 10
    MiddleMetacarpal = 11
    MiddleProximal = 12
    MiddleIntermediate = 13
    MiddleDistal = 14
    MiddleTip = 15
    RingMetacarpal = 16
    RingProximal = 17
    RingIntermediate = 18
    RingDistal = 19
    RingTip = 20
    PinkyMetacarpal = 21
    PinkyProximal = 22
    PinkyIntermediate = 23
    PinkyDistal = 24
    PinkyTip = 25
    Count = 26


def get_bones() -> np.array:
    bones = np.array([
        [HandJointIndex.Wrist.value, HandJointIndex.ThumbMetacarpal.value],
        [HandJointIndex.ThumbMetacarpal.value, HandJointIndex.ThumbProximal.value],
        [HandJointIndex.ThumbProximal.value, HandJointIndex.ThumbDistal.value],
        [HandJointIndex.ThumbDistal.value, HandJointIndex.ThumbTip.value],
        [HandJointIndex.Wrist.value, HandJointIndex.IndexMetacarpal.value],
        [HandJointIndex.IndexMetacarpal.value, HandJointIndex.IndexProximal.value],
        [HandJointIndex.IndexProximal.value, HandJointIndex.IndexIntermediate.value],
        [HandJointIndex.IndexIntermediate.value, HandJointIndex.IndexDistal.value],
        [HandJointIndex.IndexDistal.value, HandJointIndex.IndexTip.value],
        [HandJointIndex.Wrist.value, HandJointIndex.MiddleMetacarpal.value],
        [HandJointIndex.MiddleMetacarpal.value,
            HandJointIndex.MiddleProximal.value],
        [HandJointIndex.MiddleProximal.value,
            HandJointIndex.MiddleIntermediate.value],
        [HandJointIndex.MiddleIntermediate.value,
            HandJointIndex.MiddleDistal.value],
        [HandJointIndex.MiddleDistal.value, HandJointIndex.MiddleTip.value],
        [HandJointIndex.Wrist.value, HandJointIndex.RingMetacarpal.value],
        [HandJointIndex.RingMetacarpal.value, HandJointIndex.RingProximal.value],
        [HandJointIndex.RingProximal.value, HandJointIndex.RingIntermediate.value],
        [HandJointIndex.RingIntermediate.value, HandJointIndex.RingDistal.value],
        [HandJointIndex.RingDistal.value, HandJointIndex.RingTip.value],
        [HandJointIndex.Wrist.value, HandJointIndex.PinkyMetacarpal.value],
        [HandJointIndex.PinkyMetacarpal.value, HandJointIndex.PinkyProximal.value],
        [HandJointIndex.PinkyProximal.value, HandJointIndex.PinkyIntermediate.value],
        [HandJointIndex.PinkyIntermediate.value, HandJointIndex.PinkyDistal.value],
        [HandJointIndex.PinkyDistal.value, HandJointIndex.PinkyTip.value]
    ]
    )
    return bones
