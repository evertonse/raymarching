mat4 view_from_spherical(vec3 position, float theta, float phi) {
   vec3 forward = vec3(0.0, 0.0, 1.0);
   vec3 right   = vec3(1.0, 0.0, 0.0);
   vec3 up      = vec3(0.0, 1.0, 0.0);

   forward = spherical_to_cartesian(spherical.y, 0.0);

   right   = cross(up, forward);
   up      = cross(forward, right);

   // forward = normalize(forward);
   // right   = normalize(right);
   // up      = normalize(up);


   mat4 rotate = mat4(
      right.x, up.x, forward.x, 0,
      right.y, up.y, forward.y, 0,
      right.z, up.z, forward.z, 0,
      0,       0,    0,         1.0
   );

   position = camera_position;

   mat4 translate = mat4(
      1.0,         0.0,         0.0,         0.0,
      0.0,         1.0,         0.0,         0.0,
      0.0,         0.0,         1.0,         0.0,
      -position.x, -position.y, -position.z, 1.0
   );

   mat4 view = rotate * translate;
   return view;
}

mat4 lookat_rh(vec3 eye, vec3 target, vec3 up) {
    // Calculate forward vector (negative Z axis)
    vec3 f = normalize(target - eye);
    // vec3 zaxis = normalize(target);
    // Calculate right vector (X axis)
    vec3 r = normalize(cross(up, f));
    // Calculate up vector (Y axis)
    vec3 u = normalize(cross(f, r));

    // Create view matrix (column-major)
    return mat4(
       vec4(r, 0.0),
       vec4(u, 0.0),
       vec4(f, 0.0),
       vec4(-dot(r, eye), -dot(u, eye), -dot(f, eye), 1.0)
    );
}

mat4 lookat(vec3 eye, vec3 target, vec3 up) {
    vec3 f = normalize(target - eye);      // forward
    vec3 r = normalize(cross(up, f));      // right
    vec3 u = cross(f, r);                  // up (already normalized by previous step)

    // Column-major layout
    return mat4(
        vec4(r.x, u.x, f.x, 0.0),
        vec4(r.y, u.y, f.y, 0.0),
        vec4(r.z, u.z, f.z, 0.0),
        vec4(-dot(r, eye), -dot(u, eye), -dot(f, eye), 1.0)
    );
}
