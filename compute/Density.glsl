#version 460

#extension GL_ARB_compute_variable_group_size : enable

precision highp float;
precision highp int;

layout (local_size_variable) in;

layout (std430, binding = 0) buffer densityBuffer
{
    vec4 points[];
};

// Density shader inspired from : https://github.com/SebLague/Marching-Cubes/blob/master/Assets/Scripts/Compute/NoiseDensity.compute

uniform ivec3 dims;
uniform float cubeSize;
uniform vec3 offset;
uniform int octaves;
uniform float lacunarity;
uniform float persistence;
uniform float noiseScale;
uniform float noiseWeight;
uniform float floorOffset;
uniform bool closeEdges;
uniform float hardFloor;
uniform float floorWeight;
uniform float stepSize;
uniform float stepWeight;

// Simplex Noise implementation from : https://www.shadertoy.com/view/XsX3zB

/* discontinuous pseudorandom uniformly distributed in [-0.5, +0.5]^3 */
vec3 random3(vec3 c) {
	float j = 4096.0*sin(dot(c,vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0*j);
	j *= .125;
	r.x = fract(512.0*j);
	j *= .125;
	r.y = fract(512.0*j);
	return r-0.5;
}

/* skew constants for 3d simplex functions */
const float F3 =  0.3333333;
const float G3 =  0.1666667;

/* 3d simplex noise */
float simplex3d(vec3 p) {
	 /* 1. find current tetrahedron T and it's four vertices */
	 /* s, s+i1, s+i2, s+1.0 - absolute skewed (integer) coordinates of T vertices */
	 /* x, x1, x2, x3 - unskewed coordinates of p relative to each of T vertices*/

	 /* calculate s and x */
	 vec3 s = floor(p + dot(p, vec3(F3)));
	 vec3 x = p - s + dot(s, vec3(G3));

	 /* calculate i1 and i2 */
	 vec3 e = step(vec3(0.0), x - x.yzx);
	 vec3 i1 = e*(1.0 - e.zxy);
	 vec3 i2 = 1.0 - e.zxy*(1.0 - e);

	 /* x1, x2, x3 */
	 vec3 x1 = x - i1 + G3;
	 vec3 x2 = x - i2 + 2.0*G3;
	 vec3 x3 = x - 1.0 + 3.0*G3;

	 /* 2. find four surflets and store them in d */
	 vec4 w, d;

	 /* calculate surflet weights */
	 w.x = dot(x, x);
	 w.y = dot(x1, x1);
	 w.z = dot(x2, x2);
	 w.w = dot(x3, x3);

	 /* w fades from 0.6 at the center of the surflet to 0.0 at the margin */
	 w = max(0.6 - w, 0.0);

	 /* calculate surflet components */
	 d.x = dot(random3(s), x);
	 d.y = dot(random3(s + i1), x1);
	 d.z = dot(random3(s + i2), x2);
	 d.w = dot(random3(s + 1.0), x3);

	 /* multiply d by w^4 */
	 w *= w;
	 w *= w;
	 d *= w;

	 /* 3. return the sum of the four surflets */
	 return dot(d, vec4(52.0));
}


int index(ivec3 coords){
    return coords.z + dims.z * coords.y + dims.z * dims.y * coords.x;
}


void main() {
    // Get invocation id coordinates
    ivec3 coords = ivec3(gl_GlobalInvocationID);

    // Calculate the noise value for these coordinates

    vec3 pos = vec3(coords) + offset;

    float noise = 0.f;
    float frequency = noiseScale/100.f;
    float weight = 1.f;
    for(int i = 0; i < octaves; ++i){
        noise += simplex3d(pos * frequency) * weight;
        frequency *= lacunarity;
        weight *= persistence;
    }

    // Add surface features

    float finalVal = (-pos.y + floorOffset) + noise * noiseWeight + mod(pos.y, stepSize) * stepWeight;

    if(pos.y < hardFloor){
        finalVal += floorWeight;
    }

    // Add closed edges

    if(closeEdges){
        vec3 edgeOffset = abs(vec3(coords) * 2.f - dims + 1.f) - dims + 2.f;
        float edgeWeight = clamp(max(edgeOffset.x, max(edgeOffset.y, edgeOffset.z)), 0.f, 1.f);

        finalVal = finalVal * (1.f - edgeWeight) - 1000.f * edgeWeight;
    }

    // calculate the coordinates of the point
    vec3 point = vec3(coords)*cubeSize + cubeSize/2.f - dims*cubeSize/2.f;

    // store the final noise value at the correct index in the buffer
    points[index(coords)] = vec4(point, finalVal);
}
