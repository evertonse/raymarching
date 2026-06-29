mat4 matrix_translation(vec3 translationVector) {
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        translationVector.x, translationVector.y, translationVector.z, 1.0
    );
}


mat4 matrix_scale(vec3 scale) {
    return mat4(
        scale.x, 0.0,     0.0,     0.0,
        0.0,     scale.y, 0.0,     0.0,
        0.0,     0.0,     scale.z, 0.0,
        0.0,     0.0,     0.0,     1.0
    );
}

mat4 matrix_rotation(vec3 axis, float angle) {
    // Normalize the axis vector
    axis = normalize(axis);

    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;

    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return mat4(
        t * x * x + c,       t * x * y - s * z,    t * x * z + s * y,    0.0,
        t * x * y + s * z,   t * y * y + c,        t * y * z - s * x,    0.0,
        t * x * z - s * y,   t * y * z + s * x,    t * z * z + c,        0.0,
        0.0,                  0.0,                  0.0,                 1.0
    );
}

mat4 matrix_transform(vec3 translation, vec3 scale, vec4 rotation) {
    mat4 matrix = mat4(1.0);

    // Apply translation
    matrix[3] = vec4(translation, 1.0);

    // Apply rotation
    if (rotation.w != 0.0) {
        vec3 axis = normalize(rotation.xyz);
        float angle = rotation.w;

        float c = cos(angle);
        float s = sin(angle);
        float t = 1.0 - c;

        vec3 x = vec3(
            t * axis.x * axis.x + c,
            t * axis.x * axis.y - s * axis.z,
            t * axis.x * axis.z + s * axis.y
        );

        vec3 y = vec3(
            t * axis.x * axis.y + s * axis.z,
            t * axis.y * axis.y + c,
            t * axis.y * axis.z - s * axis.x
        );

        vec3 z = vec3(
            t * axis.x * axis.z - s * axis.y,
            t * axis.y * axis.z + s * axis.x,
            t * axis.z * axis.z + c
        );

        mat4 rotationMatrix = mat4(
            vec4(x, 0.0),
            vec4(y, 0.0),
            vec4(z, 0.0),
            vec4(0.0, 0.0, 0.0, 1.0)
        );

        matrix = matrix * rotationMatrix;
    }

    // Apply scale
    matrix[0][0] *= scale.x;
    matrix[1][1] *= scale.y;
    matrix[2][2] *= scale.z;

    return matrix;
}
