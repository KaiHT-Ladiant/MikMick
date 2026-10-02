import time
from pathlib import Path

from mikmick.utils.filename import expand_pattern, ext_of, unique_path


def test_counter_is_zero_padded():
    assert expand_pattern("%c", 0) == "000"
    assert expand_pattern("shot_%c", 42) == "shot_042"


def test_date_tokens():
    now = time.mktime((2026, 10, 2, 9, 41, 5, 0, 0, -1))
    assert expand_pattern("%y-%m-%d_%h%n%s", 1, now) == "2026-10-02_094105"
    assert expand_pattern("%t", 1, now) == str(int(now))


def test_unknown_token_and_literal_percent():
    assert expand_pattern("a%qb%%", 1) == "a%qb%"


def test_slashes_are_sanitized():
    assert "/" not in expand_pattern("a/b_%c", 3)


def test_empty_pattern_falls_back_to_counter():
    assert expand_pattern("", 7) == "007"


def test_unique_path(tmp_path: Path):
    first = unique_path(tmp_path, "img", "png")
    first.write_bytes(b"x")
    second = unique_path(tmp_path, "img", "png")
    assert second.name == "img (1).png"


def test_ext_of():
    assert ext_of("/tmp/A.PNG") == "png"
    assert ext_of("noext") == ""
