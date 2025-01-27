#!/usr/bin/python3
# Find the palette which minimizes the errors.

from PIL import Image
import colorsys
import numpy as np
import sys

FINAL_PALETTE = [0, 0, 0, 255, 255, 255, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 255, 127, 0]  # For 7 colors.
#FINAL_PALETTE = [0, 0, 0, 255, 255, 255, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0]  # For 6 colors.

def make_palette(s, v):
  pal = []
  pal.append(colorsys.hsv_to_rgb(0.0, 0.0, 0.1))  # Black.
  pal.append(colorsys.hsv_to_rgb(0.0, 0.0, 0.9))  # White.
  for h in [1 / 3, 2 / 3, 0.0, 1 / 6, 1 / 12]:
    pal.append(colorsys.hsv_to_rgb(h, s, v))
  #pal.pop()  # For 6 colors.
  return np.array(pal) * 255

def calc_errors(pal, img): 
  pixels = np.array(img, dtype=float).reshape((-1, 3))
  err = np.mean(np.min(np.linalg.norm(np.expand_dims(pixels, 1) - pal, axis=2), axis=1))
  return err

def main():
  assert len(sys.argv) == 3, 'usage: %s <input file> <output file>' % sys.argv[0]
  inpfile, outfile = sys.argv[1:]
  img = Image.open(inpfile)

  best_s = None
  best_v = None
  best_err = float('inf')
  for s in [0.50, 0.60, 0.70]:
    for v in [0.60, 0.65, 0.70]:
      pal = make_palette(s, v)
      err = calc_errors(pal, img)
      if err < best_err:
        best_err = err
        best_s = s
        best_v = v
  print('%s\th=%f/v=%f' % (inpfile, best_s, best_v), file=sys.stderr)
  pal = list(make_palette(best_s, best_v).reshape((-1,)).astype(int))

  tmp = Image.new('P', (1, 1))
  tmp.putpalette(pal + pal[-3:] * (256 - len(pal) // 3))
  img = img.quantize(palette=tmp)
  img.putpalette(FINAL_PALETTE + FINAL_PALETTE[-3:] * (256 - len(FINAL_PALETTE) // 3))
  img.save(outfile)

if __name__ == '__main__':
  main()
