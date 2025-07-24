mat4 matrix_translation_row(vec3 translation) {
    return mat4(
        1.0, 0.0, 0.0, translation.x,
        0.0, 1.0, 0.0, translation.y,
        0.0, 0.0, 1.0, translation.z,
        0.0, 0.0, 0.0, 1.0
    );
}

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

