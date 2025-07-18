// CORRECTED: Camera basis calculation
bool camera_basis(const Camera *camera, Vector3 *out_right, Vector3 *out_up, Vector3 *out_forward) {
   if (!camera || !out_right || !out_up || !out_forward)
      return false;

   float pitch = camera->rotation.x; // Rotation around X-axis
   float yaw = camera->rotation.y;   // Rotation around Y-axis
   float roll = camera->rotation.z;  // Rotation around Z-axis

   // CORRECTED: Forward vector calculation (negative Z in OpenGL camera space)
   // This assumes yaw=0 points along negative Z, pitch=0 is level
   Vector3 forward = {
       -sinf(yaw) * cosf(pitch), // X component
       sinf(pitch),              // Y component
       -cosf(yaw) * cosf(pitch)  // Z component (negative Z forward)
   };
   forward = Vector3Normalize(forward);

   // World up vector
   Vector3 world_up = {0, 1, 0};

   // Check if forward is too aligned with world up
   float alignment = fabsf(Vector3DotProduct(forward, world_up));
   if (alignment >= 0.999f) {
      // Use alternative up vector when looking straight up/down
      Vector3 alt_up = {0, 0, 1}; // Use Z as alternative
      Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, alt_up));
      Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

      // Apply roll
      float cos_r = cosf(roll);
      float sin_r = sinf(roll);
      *out_right = Vector3Add(Vector3Scale(right, cos_r), Vector3Scale(up, sin_r));
      *out_up = Vector3CrossProduct(*out_right, forward);
      *out_up = Vector3Normalize(*out_up);
      *out_forward = forward;
      return true;
   }

   // Standard basis calculation
   Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, world_up));
   Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

   // Apply roll rotation
   float cos_r = cosf(roll);
   float sin_r = sinf(roll);

   Vector3 right_rolled = Vector3Add(Vector3Scale(right, cos_r), Vector3Scale(up, sin_r));
   Vector3 up_rolled = Vector3Add(Vector3Scale(up, cos_r), Vector3Scale(right, -sin_r));

   *out_right = Vector3Normalize(right_rolled);
   *out_up = Vector3Normalize(up_rolled);
   *out_forward = forward;

   return true;
}

bool camera_basis2(const Camera *camera, Vector3 *out_right, Vector3 *out_up, Vector3 *out_forward) {
    if (!camera || !out_right || !out_up || !out_forward) return false;

    float pitch = camera->rotation.x;
    float yaw   = camera->rotation.y;
    float roll  = camera->rotation.z;
    // Step 1: Forward vector (Z axis)
    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    forward = Vector3Normalize(forward);

    // Base world up
    Vector3 world_up = {0, 1, 0};

    // Compute right and up from world_up (before roll)
    float alignment = fabsf(Vector3DotProduct(forward, world_up));
    if (alignment >= 0.999f) {
        return false; // Can't resolve stable basis if aligned
    }

    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, world_up));
    Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, forward));

    // Step 4: Apply roll to right and up
    float cos_r = cosf(roll);
    float sin_r = sinf(roll);

    Vector3 up_rolled = Vector3Add(Vector3Scale(up, cos_r), Vector3Scale(right, sin_r));
    Vector3 right_rolled = Vector3CrossProduct(forward, up_rolled); // Keep orthogonality

    *out_forward = forward;
    *out_right   = Vector3Normalize(right_rolled);
    *out_up      = Vector3Normalize(up_rolled);

    return true;
}
