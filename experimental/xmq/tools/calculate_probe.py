#!/usr/bin/env python3
"""Restricted, generic XMQ calculate-expression evaluator (host prototype).

No Python eval, calls, attributes or implicit unit conversions. Only supported
XMQ-compatible arithmetic with explicit input variables and unit literals.
Missing variables, unsupported syntax and divide-by-zero fail closed.
"""
import ast
import re

UNIT = {"counter": 1, "m3": 1, "h": 3600}
LITERAL = re.compile(r"(?<![A-Za-z_0-9.])(\d+(?:\.\d+)?)(counter|m3|h)\b")


class CalculateError(ValueError):
    pass


def evaluate(expression, variables):
    normalized = LITERAL.sub(lambda m: "(" + m.group(1) + "*" + str(UNIT[m.group(2)]) + ")", expression)
    try:
        tree = ast.parse(normalized, mode="eval")
    except SyntaxError as exc:
        raise CalculateError(f"Unsupported expression: {expression}") from exc

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and type(node.value) in (int, float):
            return node.value
        if isinstance(node, ast.Name):
            if node.id not in variables:
                raise CalculateError(f"Unresolved field: {node.id}")
            value = variables[node.id]
            if type(value) not in (int, float):
                raise CalculateError(f"Non-numeric field: {node.id}")
            return value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.UAdd, ast.USub)):
            value = walk(node.operand)
            return value if isinstance(node.op, ast.UAdd) else -value
        if isinstance(node, ast.BinOp):
            left, right = walk(node.left), walk(node.right)
            try:
                if isinstance(node.op, ast.Add):
                    return left + right
                if isinstance(node.op, ast.Sub):
                    return left - right
                if isinstance(node.op, ast.Mult):
                    return left * right
                if isinstance(node.op, ast.Div):
                    return left / right
                if isinstance(node.op, ast.Mod):
                    return left % right
                if isinstance(node.op, ast.RShift) and type(left) is int and type(right) is int and 0 <= right < 64:
                    return left >> right
            except (ZeroDivisionError, OverflowError) as exc:
                raise CalculateError(str(exc)) from exc
        if isinstance(node, ast.Compare):
            left = walk(node.left)
            for op, comparator in zip(node.ops, node.comparators):
                right = walk(comparator)
                if isinstance(op, ast.Lt):
                    ok = left < right
                elif isinstance(op, ast.LtE):
                    ok = left <= right
                elif isinstance(op, ast.Gt):
                    ok = left > right
                elif isinstance(op, ast.GtE):
                    ok = left >= right
                elif isinstance(op, ast.Eq):
                    ok = left == right
                elif isinstance(op, ast.NotEq):
                    ok = left != right
                else:
                    raise CalculateError("Unsupported comparison")
                if not ok:
                    return 0
                left = right
            return 1
        if isinstance(node, ast.BoolOp):
            vals = [walk(x) for x in node.values]
            if isinstance(node.op, ast.And):
                return int(all(vals))
            if isinstance(node.op, ast.Or):
                return int(any(vals))
        raise CalculateError(f"Unsupported syntax: {type(node).__name__}")

    result = walk(tree)
    if not isinstance(result, (int, float)) or abs(result) > 1e15:
        raise CalculateError("Invalid or excessively large result")
    return result
