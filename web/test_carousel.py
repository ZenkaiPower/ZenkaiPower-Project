"""Check automatic carousel timing without building the website."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
import prepare


class CarouselTiming(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='carousel-timing-')
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'figs').mkdir()
        self.root_patch = patch.object(prepare, 'ROOT', self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)

    def gif(self, delays):
        frames = [Image.new('RGB', (2, 2), color) for color in ('red', 'blue')]
        frames[0].save(self.root / 'figs/test.gif', save_all=True,
                       append_images=frames[1:], duration=delays, loop=0)

    def slide(self, extra=''):
        return prepare.carousel_slides(
            f'<img src="./figs/test.gif" data-carousel-caption="Test" {extra}/>')[0]

    def test_complete_animation_cycle(self):
        self.gif([2000, 4000])
        self.assertEqual(self.slide()['duration'], 6000)

    def test_short_loop_minimum(self):
        self.gif([100, 200])
        self.assertEqual(self.slide()['duration'], 5000)

    def test_replacing_gif_refreshes_timing(self):
        self.gif([2000, 4000])
        self.assertEqual(self.slide()['duration'], 6000)
        self.gif([4000, 5000])
        self.assertEqual(self.slide()['duration'], 9000)

    def test_explicit_override(self):
        self.gif([2000, 4000])
        self.assertEqual(self.slide('data-carousel-duration="8000"')['duration'], 8000)
        with self.assertRaises(ValueError):
            self.slide('data-carousel-duration="0"')

    def test_untagged_gif_not_included(self):
        self.gif([2000, 4000])
        self.assertEqual(prepare.carousel_slides('<img src="./figs/test.gif"/>'), [])


if __name__ == '__main__':
    unittest.main()
