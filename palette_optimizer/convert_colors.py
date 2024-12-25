#!/usr/bin/python3

from PIL import Image
import colorsys
import numpy as np
import sys

FINAL_PALETTE = [0, 0, 0, 255, 255, 255, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 255, 127, 0]
INITIAL_COLORS = np.array([(7, 7, 7), (255, 255, 255), (0, 255, 0), (0, 0, 255), (255, 0, 0), (255, 255, 0), (255, 127, 0)], float)
MAX_ITERATIONS = 10
RGB_TOLERANCE = 3
HSV_RANGE = np.array([0.03, 0.5, 0.5])
BACKGROUND_COLOR = (255, 255, 255)  # This color is ignored.

def change_color(target, original, range):
  trg_hsv = np.array(colorsys.rgb_to_hsv(*(target / 255.0)))
  org_hsv = np.array(colorsys.rgb_to_hsv(*(original / 255.0)))
  new_hsv = np.clip(trg_hsv, org_hsv - range, org_hsv + range)
  return np.array(colorsys.hsv_to_rgb(*new_hsv)) * 255.0

def optimize_palette(img): 
  pixels = []
  for y in range(img.size[1]):
    for x in range(img.size[0]):
      p = img.getpixel((x, y))
      if p != BACKGROUND_COLOR:
        pixels.append(p)
  pixels = np.array(pixels)

  colors = INITIAL_COLORS
  colors *= np.mean(pixels, axis=0) / 255.0
  for iter in range(MAX_ITERATIONS):
    print('# Iteration=%d' % iter, file=sys.stderr)
    sum_colors = colors.copy()
    num_colors = np.ones(colors.shape[0], np.int32)  # for avoiding division by zero.
    for i in range(len(pixels)):
      c = np.argmin(np.linalg.norm(colors - pixels[i], axis=1))
      sum_colors[c] += pixels[i]
      num_colors[c] += 1
    sum_colors /= num_colors[:, np.newaxis]
    new_colors = np.array([change_color(sum_colors[c], INITIAL_COLORS[c], HSV_RANGE) for c in range(len(colors))])
    print('Distribution: %s' % str(num_colors), file=sys.stderr)
    print('Palette: %s' % str(new_colors), file=sys.stderr)
    if np.allclose(new_colors, colors, atol=RGB_TOLERANCE):
      break
    colors = new_colors

  pal = list(colors.astype(int).flatten())
  pal += pal[-3:] * (256 - len(pal) // 3)
  return pal

def main():
  img = Image.open('/dev/stdin')
  pal = optimize_palette(img)
  tmp = Image.new('P', (1, 1))
  tmp.putpalette(pal)
  img = img.quantize(palette=tmp)
  img.putpalette(FINAL_PALETTE + FINAL_PALETTE[-3:] * (256 - len(FINAL_PALETTE) // 3))
  img.save('/dev/stdout', 'png')

if __name__ == '__main__':
  main()
