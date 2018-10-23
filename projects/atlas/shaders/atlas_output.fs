#version 330
in vec2 v_texCoords;

out vec4 o_fragColor;

uniform sampler2D u_texture;
uniform float u_multiplier;

vec3 tonemappingReinhard(vec3 color)
{
    return color / (color + vec3(1.0));
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    //o_fragColor = vec4(ACESFilm(u_multiplier * texture(u_texture, v_texCoords).xyz), 1.0);
    o_fragColor = vec4(tonemappingReinhard(u_multiplier * texture(u_texture, v_texCoords).xyz), 1.0);
}