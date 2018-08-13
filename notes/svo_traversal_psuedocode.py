# Pseudo code for SVO traversal based on "Efficient Sparse Voxel Octree" but without contours:
# http://www.nvidia.com/docs/IO/88972/nvr-2010-001.pdf

parent = root
idx = select_child(root, ray)
pos, scale = child_cube(root, idx)

while not_terminated:
	tc = project_cube(pos, scale, ray)
	if exists(voxel):
		if is_leaf(voxel):
			return tc.min

		# === PUSH ===
		# if tc.max < h:
		stack[scale] = parent
		# h = tc.max
		parent = find_child_descriptor(parent, idx)
		idx = select_child(pos, scale, ray)
		pos, scale = child_cube(pos, scale, idx)
		continue

	# === ADVANCE ===
	old_pos = pos
	old_idx = idx
	pos, idx = step_along_ray(pos, scale, ray)

	# === POP ===
	if not update_agrees_with_ray_signs(old_idx, idx):
		scale = highest_differing_bit(old_pos, pos)
		if scale >= s_max:
			return MISS

		parent = stack[scale]
		pos = round_position(pos, scale)
		idx = extract_child_slot_index(pos, scale)
		# h = 0

