#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

uniform vec3 lightColor;
uniform vec3 toyColor;

void main()
{
    gl_FragColor =vec4(lightColor * toyColor,1.0);
}

