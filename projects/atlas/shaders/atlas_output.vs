#version 330
layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 texCoords;

out vec2 v_texCoords;

void main(){
	v_texCoords = texCoords;
	gl_Position = vec4(pos, 1.0);
}