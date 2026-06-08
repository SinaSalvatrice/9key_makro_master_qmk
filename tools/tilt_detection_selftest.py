"""Self-test for ADXL345 tilt up/down detection logic.

This mirrors the firmware logic in keymaps/reworked/keymap.c:
- adxl345_axis_desired_state()
- adxl345_axis_update_state() (pending + debounce)

It intentionally does NOT depend on QMK or a toolchain, so you can run it
anywhere Python is available.

Run:
  python tools/tilt_detection_selftest.py

Exit code:
  0 on success, 1 on failure.
"""

from __future__ import annotations

from dataclasses import dataclass


def sign_i16(value: int) -> int:
    if value > 0:
        return 1
    if value < 0:
        return -1
    return 0


def abs_i16_u16(value: int) -> int:
    return abs(int(value))


def axis_desired_state(value: int, current_state: int, on_threshold: int, off_threshold: int) -> int:
    magnitude = abs_i16_u16(value)
    sign = sign_i16(value)

    if current_state == 0:
        if sign == 0 or magnitude < on_threshold:
            return 0
        return sign

    if magnitude <= off_threshold:
        return 0
    if sign == 0 or sign == current_state:
        return current_state

    # If we cross through zero with enough magnitude, switch immediately.
    return sign if magnitude >= on_threshold else current_state


@dataclass
class AxisState:
    state: int = 0
    pending: int = 0
    pending_since_ms: int | None = None


def axis_update_state(
    value: int,
    axis: AxisState,
    now_ms: int,
    debounce_ms: int,
    on_threshold: int,
    off_threshold: int,
) -> int:
    desired = axis_desired_state(value, axis.state, on_threshold, off_threshold)
    if desired == axis.state:
        axis.pending = axis.state
        axis.pending_since_ms = None
        return axis.state

    if axis.pending_since_ms is None or axis.pending != desired:
        axis.pending = desired
        axis.pending_since_ms = now_ms
        return axis.state

    if (now_ms - axis.pending_since_ms) >= debounce_ms:
        axis.state = desired
        axis.pending_since_ms = None

    return axis.state


def assert_eq(name: str, actual: int, expected: int) -> None:
    if actual != expected:
        raise AssertionError(f"{name}: expected {expected}, got {actual}")


def step(axis: AxisState, values: list[int], debounce_ms: int, on_th: int, off_th: int, dt_ms: int = 10) -> list[int]:
    out: list[int] = []
    now = 0
    for v in values:
        out.append(axis_update_state(v, axis, now, debounce_ms, on_th, off_th))
        now += dt_ms
    return out


def main() -> None:
    # These are roughly aligned with the firmware defaults.
    ON = 58
    OFF = 34
    DEBOUNCE = 60

    # 1) Idle noise should not trigger.
    a = AxisState()
    out = step(a, [0, 5, -10, 20, -33, 33, 0], DEBOUNCE, ON, OFF)
    assert_eq("idle_noise_end", out[-1], 0)

    # 2) Tilt up: hold value above ON long enough => state becomes +1.
    a = AxisState()
    # 7 samples * 10ms = 70ms >= 60ms debounce
    out = step(a, [0, 10, 30, 60, 60, 60, 60], DEBOUNCE, ON, OFF)
    assert_eq("tilt_up_final", out[-1], 1)

    # 3) Return towards center: below OFF long enough => back to 0.
    out = step(a, [60, 40, 33, 20, 10, 0, 0], DEBOUNCE, ON, OFF)
    assert_eq("tilt_release_final", out[-1], 0)

    # 4) Tilt down: hold value below -ON long enough => state becomes -1.
    a = AxisState()
    out = step(a, [0, -10, -40, -60, -60, -60, -60], DEBOUNCE, ON, OFF)
    assert_eq("tilt_down_final", out[-1], -1)

    # 5) Quick flip across zero without enough time should not instantly chatter.
    a = AxisState()
    out = step(a, [60, -60, 60, -60, 60], DEBOUNCE, ON, OFF)
    # still 0 because pending never debounced
    assert_eq("flip_chatter", out[-1], 0)

    print("OK: tilt detection hysteresis/debounce self-test passed")


if __name__ == "__main__":
    main()
