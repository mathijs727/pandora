import subprocess
import os

# https://stackoverflow.com/questions/40590192/getting-an-error-attributeerror-module-object-has-no-attribute-run-while
def run(*popenargs, input=None, check=False, **kwargs):
	if input is not None:
		if 'stdin' in kwargs:
			raise ValueError('stdin and input arguments may not both be used.')
		kwargs['stdin'] = subprocess.PIPE

	process = subprocess.Popen(*popenargs, **kwargs)
	try:
		stdout, stderr = process.communicate(input)
	except:
		process.kill()
		process.wait()
		raise
	retcode = process.poll()
	if check and retcode:
		raise subprocess.CalledProcessError(
			retcode, process.args, output=stdout, stderr=stderr)
	return retcode, stdout, stderr

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

	run(["git", "apply", patch_path], cwd=tinyply_path)

if __name__ == "__main__":
	#fix_eastl()
	#print("EASTL fixed")

	fix_tinyply()
	print("tinyply fixed")
