#ifdef GL_ES
// Set default precision to medium
precision mediump int;
precision mediump float;
#endif

struct Material
{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

struct Light {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

varying vec3 fragPos;
varying vec3 Normal;

uniform vec3 viewPos;
uniform Material material;
uniform Light light1;
uniform Light light2;

varying vec3 mater;

vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * material.ambient;
    vec3 diffuse = light.diffuse * diff * material.diffuse;
    vec3 specular = light.specular * spec * material.specular;
    return (ambient + diffuse + specular);
}

void main()
{

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 result = CalcDirLight(light1, norm, viewDir);
    gl_FragColor = vec4(mater*result, 1.0);
   // gl_FragColor = vec4(mater*vec3(0.7,0.8,0.7), 1.0);
}








