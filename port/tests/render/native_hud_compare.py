"""Metric checks for compare_native_hud.py; no GPU required."""
from pathlib import Path
import sys
import unittest
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'tools'))
from compare_native_hud import compare

class ComparisonMetrics(unittest.TestCase):
    def test_invisible_rgb_is_ignored(self):
        old=Image.new('RGBA',(2,1));old.putdata([(255,0,0,255),(0,255,0,0)])
        new=Image.new('RGBA',(2,1));new.putdata([(255,0,0,255),(0,0,255,0)])
        metrics,_=compare(old,new)
        self.assertEqual(metrics['active_pixels'],1)
        self.assertEqual(metrics['premultiplied_rgb_mae_0_255'],0)
        self.assertEqual(metrics['alpha_mae_0_255'],0)
        self.assertEqual(metrics['max_channel_error_0_255'],0)
        self.assertEqual(metrics['silhouette_iou_alpha_ge_16'],1)

    def test_color_error(self):
        metrics,_=compare(Image.new('RGBA',(1,1),(255,0,0,255)),
                          Image.new('RGBA',(1,1),(0,255,0,255)))
        self.assertEqual(metrics['premultiplied_rgb_mae_0_255'],170)
        self.assertEqual(metrics['alpha_mae_0_255'],0)
        self.assertEqual(metrics['max_channel_error_0_255'],255)

    def test_alpha_and_missing_shape(self):
        old=Image.new('RGBA',(1,1),(255,0,0,255))
        new=Image.new('RGBA',(1,1),(255,0,0,0))
        metrics,_=compare(old,new)
        self.assertEqual(metrics['premultiplied_rgb_mae_0_255'],85)
        self.assertEqual(metrics['alpha_mae_0_255'],255)
        self.assertEqual(metrics['silhouette_iou_alpha_ge_16'],0)
        self.assertEqual(metrics,compare(new,old)[0])

    def test_empty_images(self):
        metrics,_=compare(Image.new('RGBA',(1,1)),Image.new('RGBA',(1,1)))
        self.assertEqual(metrics['active_pixels'],0)
        self.assertEqual(metrics['premultiplied_rgb_rmse_0_255'],0)
        self.assertEqual(metrics['silhouette_iou_alpha_ge_16'],1)

if __name__=='__main__':
    unittest.main()
