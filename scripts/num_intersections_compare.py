import argparse
import matplotlib.pyplot as plt
import numpy as np
import os
import shared_image

def check_file_exists(file):
	if not os.path.exists(file) or not os.path.isfile(file):
		print(f"File \"{file}\" does not exist!")
		exit(1)

if __name__ == "__main__":
	parser = argparse.ArgumentParser("EXRViewer")
	parser.add_argument("file1", type=str)
	parser.add_argument("file2", type=str)
	parser.add_argument("--spp", type=float, default=1)
	args = parser.parse_args()

	check_file_exists(args.file1)
	check_file_exists(args.file2)

	image1 = shared_image.read_exr(args.file1) / args.spp
	image2 = shared_image.read_exr(args.file2) / args.spp

	vmax = max(np.max(image1), np.max(image2))

	fig, (ax1, ax2) = plt.subplots(1, 2)
	ax1.imshow(image1, vmin=0, vmax=vmax)
	pos2 = ax2.imshow(image2, vmin=0, vmax=vmax)
	fig.colorbar(pos2, ax=ax2)
	plt.show()