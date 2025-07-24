
mat2 rotation(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}
