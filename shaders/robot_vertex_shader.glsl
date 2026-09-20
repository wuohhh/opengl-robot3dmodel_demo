#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform mat4 mvp_matrix;
uniform mat4 model;

attribute vec3 a_position;
attribute vec3 a_material;
attribute vec3 a_normal;

varying vec3 mater;
varying vec3 Normal;
varying vec3 fragPos;

//! [0]
void main()
{
    // Calculate vertex position in screen space
    //gl_Position = mvp_matrix * a_position;
    gl_Position = mvp_matrix * vec4(a_position,1.0);
    fragPos =vec3((model*vec4(a_position,1.0)));
    //normal=a_normal;
    Normal=vec3(model*vec4(a_normal,0.0));

    // Pass texture coordinate to fragment shader
    // Value will be automatically interpolated to fragments inside polygon faces
    mater=a_material;
}
//! [0]
