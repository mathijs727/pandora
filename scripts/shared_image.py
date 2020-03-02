import OpenImageIO as oiio
import os

def read_exr(file):
	if not os.path.exists(file) or not os.path.isfile(file):
		print(f"File \"{file}\" does not exist!")
		exit(1)

	image = oiio.ImageInput.open(file)
	spec = image.spec()
	if spec.channelformat(0) == "uint":
		pixels = image.read_image("UINT32")
	else:
		pixels = image.read_image()
	image.close()

	if pixels.shape[2] == 1:
		return pixels.reshape(pixels.shape[0], -1)
	else:
		return pixels