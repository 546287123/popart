import numpy as np
import popart
import torch
import pytest
from op_tester import op_tester
import sys
from pathlib import Path
sys.path.append(Path(__file__).resolve().parent.parent)
import test_util as tu

import matmul_test_broadcasting_base as mtb

# generated test cases
# these are all known to be valid input shapes to np.matmul
shapes_ = (
    ([1], [4, 2, 1, 3]),
    ([4, 6, 2], [4, 2, 8]),
    ([6, 2], [4, 2, 8]),
    ([2], [4, 2, 8]),
    ([2, 1], [1, 2]),
    ([1, 2], [4, 2, 1]),
    ([2], [4, 2, 1]),
    ([1, 4, 2], [2]),
    ([2, 2], [2, 2]),
    ([1, 3, 4], [1, 3, 4, 2]),
    ([2, 1, 4], [1, 4, 3]),
    ([3], [3]),
    ([2, 3, 1, 4], [1, 4, 3]),
    ([2, 4, 1, 3], [2, 4, 3, 1]),
    ([3, 1, 2], [2, 4]),
    ([1, 2], [4, 3, 2, 1]),
    ([2, 1, 4, 3], [1, 2, 3, 4]),
)


def test_matmul_broadcasting_1(op_tester):
    mtb._test_matmul_broadcasting_base(op_tester, shapes_)
