import numpy as np
import pytest
from PySide6.QtCore import QRect
from PySide6.QtGui import QColor, QImage

from mikmick.editor import effects


@pytest.fixture()
def img(qapp):
    im = QImage(40, 30, QImage.Format.Format_ARGB32)
    im.fill(QColor(200, 100, 50))
    return im


def test_array_roundtrip(img):
    a = effects.to_array(img)
    assert a.shape == (30, 40, 4)
    assert tuple(a[0, 0]) == (50, 100, 200, 255)
    back = effects.from_array(a)
    assert back.pixelColor(5, 5) == QColor(200, 100, 50)


def test_grayscale(img):
    out = effects.grayscale(img)
    c = out.pixelColor(0, 0)
    assert c.red() == c.green() == c.blue()


def test_invert(img):
    assert effects.invert(img).pixelColor(1, 1) == QColor(55, 155, 205)


def test_region_only(img):
    out = effects.invert(img, QRect(0, 0, 10, 10))
    assert out.pixelColor(1, 1) == QColor(55, 155, 205)
    assert out.pixelColor(20, 20) == QColor(200, 100, 50)


def test_blur_keeps_uniform_color(img):
    c = effects.blur(img, 3).pixelColor(20, 15)
    assert abs(c.red() - 200) <= 1 and abs(c.green() - 100) <= 1


def test_mosaic_averages_blocks(qapp):
    im = QImage(4, 4, QImage.Format.Format_ARGB32)
    im.fill(QColor(0, 0, 0))
    im.setPixelColor(0, 0, QColor(255, 255, 255))
    out = effects.mosaic(im, 2)
    assert out.pixelColor(1, 1).red() == pytest.approx(64, abs=1)
    assert out.pixelColor(3, 3).red() == 0


def test_rotate_and_flip(img):
    assert effects.rotate(img, 90).size().width() == 30
    flipped = effects.flip(img, True)
    assert flipped.size() == img.size()


def test_border_grows_image(img):
    out = effects.border(img, 5, QColor("black"))
    assert (out.width(), out.height()) == (50, 40)
    assert out.pixelColor(0, 0) == QColor("black")
    inside = effects.border(img, 5, QColor("black"), inside=True)
    assert inside.size() == img.size()


def test_canvas_size_anchor(img):
    out = effects.canvas_size(img, 60, 50, "top-left", QColor("white"))
    assert out.pixelColor(0, 0) == QColor(200, 100, 50)
    assert out.pixelColor(59, 49) == QColor("white")


def test_drop_shadow_grows(img):
    out = effects.drop_shadow(img, offset=4, radius=2)
    assert out.width() > img.width() and out.height() > img.height()


def test_hue_saturation_identity(img):
    out = effects.hue_saturation(img, 0, 0, 0)
    c = out.pixelColor(3, 3)
    assert abs(c.red() - 200) <= 1 and abs(c.green() - 100) <= 1 and abs(c.blue() - 50) <= 1


def test_brightness(img):
    out = effects.brightness_contrast(img, 100, 0)
    assert out.pixelColor(0, 0).red() == 255


def test_flood_fill_stops_at_border(qapp):
    im = QImage(10, 10, QImage.Format.Format_ARGB32)
    im.fill(QColor("white"))
    for y in range(10):
        im.setPixelColor(5, y, QColor("black"))
    out = effects.flood_fill(im, 1, 1, QColor("red"), 0)
    assert out.pixelColor(0, 9) == QColor("red")
    assert out.pixelColor(5, 5) == QColor("black")
    assert out.pixelColor(8, 8) == QColor("white")
    a = effects.to_array(out)
    assert int(np.sum(a[..., 2] == 255) - np.sum(a[..., 1] == 255)) == 50


def test_watermark_changes_pixels(qapp):
    im = QImage(200, 100, QImage.Format.Format_ARGB32)
    im.fill(QColor("black"))
    out = effects.watermark(im, "MikMick", QColor("white"), 1.0, "center", 30)
    assert effects.to_array(out)[..., :3].max() > 0
