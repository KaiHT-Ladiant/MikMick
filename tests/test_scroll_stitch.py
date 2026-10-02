import numpy as np

from mikmick.capture.scroll import find_overlap, stitch


def _page(height=300, width=20, seed=1):
    rng = np.random.default_rng(seed)
    return rng.integers(0, 2**32, size=(height, width), dtype=np.uint32)


def test_find_overlap_detects_scroll_distance():
    page = _page()
    prev = page[0:100]
    cur = page[30:130]
    assert find_overlap(prev, cur) == 70


def test_find_overlap_no_change_returns_none():
    page = _page()
    assert find_overlap(page[0:100], page[0:100].copy()) is None


def test_stitch_reconstructs_page():
    page = _page()
    frames = [page[i : i + 100] for i in (0, 40, 80, 120, 160, 200, 200)]
    out = stitch(frames)
    assert out.shape == (300, 20)
    assert np.array_equal(out, page)
