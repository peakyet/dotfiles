#!/usr/bin/env python3
"""Waybar clock text plus a calendar with Chinese public holidays highlighted.

Holiday data follows the official 2026 State Council schedule:
https://www.gov.cn/zhengce/zhengceku/202511/content_7047091.htm
"""

from __future__ import annotations

import calendar
import json
from datetime import date, datetime
from zoneinfo import ZoneInfo

TIMEZONE = ZoneInfo("Asia/Shanghai")
TODAY_COLOR = "#0A84FF"
HOLIDAY_COLOR = "#FF453A"
WEEKEND_COLOR = "#FF9F0A"

# Official 2026 holiday periods. Update this table when the next annual
# schedule is published.
HOLIDAY_PERIODS = (
    (date(2026, 1, 1), date(2026, 1, 3), "元旦"),
    (date(2026, 2, 15), date(2026, 2, 23), "春节"),
    (date(2026, 4, 4), date(2026, 4, 6), "清明节"),
    (date(2026, 5, 1), date(2026, 5, 5), "劳动节"),
    (date(2026, 6, 19), date(2026, 6, 21), "端午节"),
    (date(2026, 9, 25), date(2026, 9, 27), "中秋节"),
    (date(2026, 10, 1), date(2026, 10, 7), "国庆节"),
)

# Saturdays/Sundays that the official schedule turns into working days.
MAKEUP_WORKDAYS = frozenset(
    {
        date(2026, 1, 4),
        date(2026, 2, 14),
        date(2026, 2, 28),
        date(2026, 5, 9),
        date(2026, 9, 20),
        date(2026, 10, 10),
    }
)


def holiday_name(day: date) -> str | None:
    for start, end, name in HOLIDAY_PERIODS:
        if start <= day <= end:
            return name
    return None


def day_markup(day: int, today: date, year: int, month: int) -> str:
    value = f"{day:2d}"
    current_date = date(year, month, day)
    current = current_date == today
    holiday = holiday_name(current_date)

    if holiday:
        underline = " underline='single'" if current else ""
        return (
            f"<span foreground='{HOLIDAY_COLOR}' weight='bold'{underline}>"
            f"{value}</span>"
        )
    if current:
        return f"<span foreground='{TODAY_COLOR}' weight='bold'>{value}</span>"
    if current_date.weekday() >= calendar.SATURDAY and current_date not in MAKEUP_WORKDAYS:
        return f"<span foreground='{WEEKEND_COLOR}'>{value}</span>"
    return value


def month_tooltip(today: date) -> str:
    month_name = calendar.month_name[today.month]
    weeks = calendar.Calendar(firstweekday=6).monthdayscalendar(today.year, today.month)

    lines = [f"{month_name} {today.year}", "Su Mo Tu We Th Fr Sa"]
    for week in weeks:
        cells = [
            day_markup(day, today, today.year, today.month) if day else "  "
            for day in week
        ]
        lines.append(" ".join(cells).rstrip())

    holiday_names = []
    for day in range(1, calendar.monthrange(today.year, today.month)[1] + 1):
        name = holiday_name(date(today.year, today.month, day))
        if name and name not in holiday_names:
            holiday_names.append(name)
    if holiday_names:
        lines.append("")
        lines.append(
            f"<span foreground='{HOLIDAY_COLOR}'>●</span> "
            + "、".join(holiday_names)
            + f"   <span foreground='{WEEKEND_COLOR}'>●</span> 周末"
        )

    calendar_text = "\n".join(lines)
    return (
        f"<big>{today.year} {month_name}</big>\n"
        f"<tt><small>{calendar_text}</small></tt>"
    )


def main() -> None:
    now = datetime.now(TIMEZONE)
    print(
        json.dumps(
            {
                "text": now.strftime("%a %d %b  %H:%M"),
                "tooltip": month_tooltip(now.date()),
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
