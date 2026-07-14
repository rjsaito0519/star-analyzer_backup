#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Convert New York time to Japan Standard Time (JST).
Handles EST/EDT (America/New_York) automatically.
Usage:
  python script/time_NYT_to_JST.py [NY_DATETIME]
  python script/time_NYT_to_JST.py "2025-02-18 14:30"
  python script/time_NYT_to_JST.py "2025-02-18 14:30:00"
  python script/time_NYT_to_JST.py   # prints usage

Input format: YYYY-MM-DD HH:MM or YYYY-MM-DD HH:MM:SS
Output: YYYY-MM-DD HH:MM JST
"""
from __future__ import print_function
import sys
import argparse
from datetime import datetime, timedelta, time

try:
    from zoneinfo import ZoneInfo
    _USE_ZONEINFO = True
except ImportError:
    try:
        import pytz
        _USE_PYTZ = True
        _USE_ZONEINFO = False
    except ImportError:
        _USE_ZONEINFO = False
        _USE_PYTZ = False

NY_TZ = "America/New_York"
JST_TZ = "Asia/Tokyo"
JST_OFFSET = timedelta(hours=9)

FORMATS = ["%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M", "%Y-%m-%d"]


def _is_dst_edt(dt):
    """True if dt is in EDT (UTC-4), else EST (UTC-5). US Eastern rules."""
    y = dt.year
    # 2nd Sunday in March, 2am local
    march1 = datetime(y, 3, 1)
    march_second_sunday = march1 + timedelta(days=(6 - march1.weekday()) % 7 + 7)
    dt_start = march_second_sunday.replace(hour=2, minute=0, second=0, microsecond=0)
    # 1st Sunday in November, 2am local
    nov1 = datetime(y, 11, 1)
    nov_first_sunday = nov1 + timedelta(days=(6 - nov1.weekday()) % 7)
    dt_end = nov_first_sunday.replace(hour=2, minute=0, second=0, microsecond=0)
    return dt_start <= dt < dt_end


def _ny_to_jst_fallback(dt):
    """Fallback: NY local -> JST using manual EST/EDT offset."""
    if _is_dst_edt(dt):
        utc_offset = timedelta(hours=-4)
    else:
        utc_offset = timedelta(hours=-5)
    utc_dt = dt - utc_offset
    jst_dt = utc_dt + JST_OFFSET
    return jst_dt.strftime("%Y-%m-%d %H:%M JST")


def ny_to_jst(dt_str):
    """
    Convert NY time string to JST.
    Args:
        dt_str: datetime string (YYYY-MM-DD HH:MM or YYYY-MM-DD HH:MM:SS)
    Returns:
        str: "YYYY-MM-DD HH:MM JST"
    """
    dt = None
    for fmt in FORMATS:
        try:
            dt = datetime.strptime(dt_str.strip(), fmt)
            break
        except ValueError:
            continue
    if dt is None:
        raise ValueError(
            "Unsupported format. Use YYYY-MM-DD HH:MM or YYYY-MM-DD HH:MM:SS"
        )

    if _USE_ZONEINFO:
        ny_tz = ZoneInfo(NY_TZ)
        jst_tz = ZoneInfo(JST_TZ)
        ny_dt = dt.replace(tzinfo=ny_tz)
        jst_dt = ny_dt.astimezone(jst_tz)
        return jst_dt.strftime("%Y-%m-%d %H:%M JST")
    elif _USE_PYTZ:
        ny_tz = pytz.timezone(NY_TZ)
        jst_tz = pytz.timezone(JST_TZ)
        ny_dt = ny_tz.localize(dt)
        jst_dt = ny_dt.astimezone(jst_tz)
        return jst_dt.strftime("%Y-%m-%d %H:%M JST")
    else:
        return _ny_to_jst_fallback(dt)


def main():
    parser = argparse.ArgumentParser(
        description="Convert New York time to JST (YYYY-MM-DD HH:MM JST)"
    )
    parser.add_argument(
        "ny_datetime",
        nargs="?",
        default=None,
        help="NY datetime (e.g. 2025-02-18 14:30)",
    )
    parser.add_argument(
        "-q", "--quiet",
        action="store_true",
        help="Suppress error messages to stderr",
    )
    args = parser.parse_args()

    if not args.ny_datetime:
        parser.print_help()
        sys.exit(0)

    try:
        result = ny_to_jst(args.ny_datetime)
        print(result)
    except ValueError as e:
        if not args.quiet:
            print("ERROR: {}".format(e), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
