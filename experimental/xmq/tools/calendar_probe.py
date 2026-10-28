#!/usr/bin/env python3
"""Calendar expressions from XMQ heat-allocation drivers (host experiment).

Uses original XMQ arithmetic expressions for intermediate fields; applies
calendar-month and 24-hour date offsets without approximating month lengths.
"""
import calendar
import re
from datetime import date, timedelta
from pathlib import Path
from calculate_probe import evaluate, CalculateError
from portable_xmq import parse, validate_driver, blocks, values


class CalendarError(ValueError):
    pass


def formulas(path):
    d = validate_driver(parse(Path(path).read_text(encoding="utf-8")))
    result = {}
    for group in blocks(d, "fields"):
        for field in blocks(group["entries"], "field"):
            entries = field["entries"]
            expression = values(entries, "calculate")
            if expression:
                result[values(entries, "name")[0]] = (expression[0], values(entries, "display_unit"))
    return result


def month_offset(base, months):
    if type(months) is not int or abs(months) > 10000:
        raise CalendarError("Invalid month offset")
    year, month0 = divmod(base.year * 12 + base.month - 1 + months, 12)
    if not 1 <= year <= 9999:
        raise CalendarError("Date outside supported range")
    month = month0 + 1
    return date(year, month, min(base.day, calendar.monthrange(year, month)[1]))


def date_expression(expression, env):
    # This is a deliberately bounded subset matching XMQ date expressions:
    # date literal + numeric expression * 1month + numeric expression * 24h.
    pattern = re.compile(
        r"""^\s*['"](?P<base>\d{4}-\d{2}-\d{2})['"]\s*\+\s*
        (?P<months>\(.+\))\s*\*\s*1month\s*\+\s*
        (?P<days>\(.+\))\s*\*\s*24h\s*$""",
        re.X | re.S,
    )
    match = pattern.match(expression)
    if not match:
        raise CalendarError("Unsupported XMQ date formula")
    months = evaluate(match.group("months"), env)
    days = evaluate(match.group("days"), env)
    if int(months) != months or int(days) != days:
        raise CalendarError("Fractional calendar offset")
    result = month_offset(date.fromisoformat(match.group("base")), int(months))
    return (result + timedelta(days=int(days))).isoformat()


def compute_dates(driver_path, captured):
    expressions = formulas(driver_path)
    env = dict(captured)
    # Numeric XMQ calculate fields refer to other results with _counter suffix.
    required = ("previous_day", "previous_month", "previous_year",
                "current_day", "current_month", "current_year")
    pending = list(required)
    while pending:
        remaining = []
        progress = False
        for name in pending:
            expr = expressions[name][0].replace("&&", " and ").replace("||", " or ")
            try:
                result = evaluate(expr, env)
            except CalculateError as exc:
                if "Unresolved field:" in str(exc):
                    remaining.append(name)
                    continue
                raise
            env[name + "_counter"] = result
            progress = True
        if not progress:
            raise CalendarError("Unresolved calculate dependencies: " + ",".join(remaining))
        pending = remaining
    return {
        name + "_date": date_expression(expressions[name][0], env)
        for name in ("previous", "current")
    }
