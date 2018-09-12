import subprocess
import os

def fix_eastl_compiler_traits():
	filename = "./third_party/eastl/test/packages/EABase/include/Common/EABase/config/eacompilertraits.h"
	with open(filename, "r") as f:
		contents = f.read()

	contents = contents.replace("EA_DISABLE_VC_WARNING(4822);", "EA_DISABLE_VC_WARNING(4822)")
	contents = contents.replace("EA_RESTORE_VC_WARNING();", "EA_RESTORE_VC_WARNING()")

	with open(filename, "w") as f:
		f.write(contents)

def fix_eastl():
	fix_eastl_compiler_traits()

def fix_tinyply():
	tinyply_path = os.path.abspath("./third_party/tinyply/")
	patch_path = os.path.abspath("./travis/tinyply_fix.patch")

	subprocess.run(["git", "apply", patch_path], cwd=tinyply_path)

if __name__ == "__main__":
	#fix_eastl()
	#print("EASTL fixed")

	fix_tinyply()
	print("tinyply fixed")