#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Get current time on the analysis server (NY) and convert to JST.
Uses America/New_York timezone for the server time, so it works correctly
whether run locally (Japan) or on the server (NY).
Usage:
  python script/time_now_NY_to_JST.py

Output: YYYY-MM-DD HH:MM JST
"""
from __future__ import print_function
import os
import sys

# Add script dir to path so we can import time_NYT_to_JST
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

from datetime import datetime, timedelta

try:
    from zoneinfo import ZoneInfo
    _HAS_ZONEINFO = True
except ImportError:
    try:
        import pytz
        _HAS_PYTZ = True
        _HAS_ZONEINFO = False
    except ImportError:
        _HAS_ZONEINFO = False
        _HAS_PYTZ = False

from time_NYT_to_JST import ny_to_jst, _is_dst_edt

NY_TZ = "America/New_York"


def get_current_ny_time_str():
    """Get current time in NY timezone as string for ny_to_jst()."""
    if _HAS_ZONEINFO:
        ny_tz = ZoneInfo(NY_TZ)
        now_ny = datetime.now(ny_tz)
        return now_ny.strftime("%Y-%m-%d %H:%M:%S")
    elif _HAS_PYTZ:
        import pytz
        ny_tz = pytz.timezone(NY_TZ)
        now_ny = datetime.now(ny_tz)
        return now_ny.strftime("%Y-%m-%d %H:%M:%S")
    else:
        utc_now = datetime.utcnow()
        # Try EST first; if in EDT, use -4
        ny_est = utc_now - timedelta(hours=5)
        ny_offset = timedelta(hours=-4) if _is_dst_edt(ny_est) else timedelta(hours=-5)
        now_ny = utc_now + ny_offset
        return now_ny.strftime("%Y-%m-%d %H:%M:%S")


def main():
    ny_str = get_current_ny_time_str()
    jst_str = ny_to_jst(ny_str)
    print(jst_str)


if __name__ == "__main__":
    main()
