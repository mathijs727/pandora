import argparse
import OpenImageIO as oiio
import matplotlib.pyplot as plt
import numpy as np

if __name__ == "__main__":
	parser = argparse.ArgumentParser("EXRViewer")
	parser.add_argument("file", type=str)
	parser.add_argument("--max", type=float)
	args = parser.parse_args()

	image = oiio.ImageInput.open(args.file)
	spec = image.spec()
	if spec.channelformat(0) == "uint":
		pixels = image.read_image("UINT32")
	else:
		pixels = image.read_image()
	image.close()

	if pixels.shape[2] == 1:
		plt_pixels = pixels.reshape(pixels.shape[0], -1)
	else:
		plt_pixels = pixels
	
	plt_pixels = plt_pixels / 8

	if args.max:
		vmax = args.max
	else:
		vmax = np.max(plt_pixels)
		print(f"Max = {vmax}")

	plt.imshow(plt_pixels, vmin=0, vmax=vmax)
	plt.show()