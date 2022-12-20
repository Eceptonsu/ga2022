#version 450

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 Fragpos;
layout (location = 2) in vec3 Normal;

layout (location = 0) out vec4 outFragColor;

void main()
{

    vec3 lightPos = vec3(0.0, 0.0, 0.0); 
    vec3 viewPos = vec3(0.0, 0.0, 0.0); 
    vec3 lightColor = vec3(1.0, 1.0, 1.0);

    // ambient
    float ambientStrength = 0.5;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - Fragpos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - Fragpos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec4 result = vec4((ambient + diffuse + specular), 1.0) * inColor;
    outFragColor = result;
}