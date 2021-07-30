#!/usr/bin/env python

from __future__ import division, print_function, unicode_literals

import re
from typing import Any

import ttfw_idf


@ttfw_idf.idf_example_test(env_tag='Example_GENERIC', target=['esp32', 'esp32s2', 'esp32c3'])
def test_GBPlay(env, _):  # type: (Any, Any) -> None
    app_name = 'GBPlay'
    dut = env.get_dut(app_name, 'GBPlay')
    dut.start_app()


if __name__ == '__main__':
    test_GBPlay()
