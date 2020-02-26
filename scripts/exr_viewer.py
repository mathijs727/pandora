import argparse
import matplotlib.pyplot as plt
import numpy as np
import os
import shared_image

if __name__ == "__main__":
	parser = argparse.ArgumentParser("EXRViewer")
	parser.add_argument("file", type=str)
	parser.add_argument("--max", type=float)
	parser.add_argument("--scale", type=float, default=1)
	args = parser.parse_args()

	pixels = shared_image.read_exr(args.file)
	pixels = pixels * args.scale

	if args.max:
		vmax = args.max
	else:
		vmax = np.max(pixels)
		print(f"Max = {vmax}")

	plt.imshow(pixels, vmin=0, vmax=vmax)
	plt.show()