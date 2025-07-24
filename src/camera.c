
// Pull out camera to camera file (gameplay folder?)
typedef struct {
   Vector3 position;
   Vector3 rotation; // .x value is radians rotation around x-axis
   f32 zoom;
} Camera;
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

Camera move_camera(Camera cam) {

   ////////////////////////
   //// Rotation //////////
   ////////////////////////
   static bool mouse_button_left_down = false;
   static Vector2 mouse_last_position = { .x = -1.0f, .y = -1.0f };

   Vector2 cursor = cursor_position();
   f64 mouse_x = cursor.x, mouse_y = cursor.y;
   const f32 sensitivity = 60.0;
   f32 sensitivity_factor = Remap(sensitivity, 0.0f, 100.0f, 0.0059f, 0.0009f);
   if (is_button_pressed(BUTTON_MOUSE_LEFT)) {
      if (!mouse_button_left_down) {
         // First time pressing the button, store the last position
         mouse_button_left_down = true;
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      } else {
         // Calculate the mouse movement
         f32 delta_x = (f32)(mouse_x - mouse_last_position.x);
         f32 delta_y = (f32)(mouse_y - mouse_last_position.y);

         // Update camera rotation based on mouse movement
         cam.rotation.y -= delta_x * sensitivity_factor;
         cam.rotation.x -= delta_y * sensitivity_factor;

         // Clamp the vertical rotation to prevent flipping
         if (cam.rotation.x > DEG2RAD * (89.0f))
            cam.rotation.x = DEG2RAD * (89.0f);
         if (cam.rotation.x < DEG2RAD * (-89.0f))
            cam.rotation.x = DEG2RAD * (-89.0f);

         // Update the last mouse position
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      }
   } else if (is_button_released(BUTTON_MOUSE_LEFT)) {
      mouse_button_left_down = false;
   }

   ////////////////////////
   //// Position //////////
   ////////////////////////

   Vector3 v;
   v.x = 0;
   v.y = 0;
   v.z = 0;

   Vector3 forward = Vector3RotateByAxisAngle((Vector3){0., 0., 1.}, (Vector3){0., 1., 0.}, -cam.rotation.y);
   Vector3 right   = cross(forward, (Vector3){0., 1., 0.});
   forward = normalize(forward);
   right   = normalize(right);

   Vector3 up = normalize(cross(right, forward));


   f32 speed = 20.20f ;
   if (is_button_pressed(BUTTON_W)) {
      v = add(v, forward);
   }

   if (is_button_pressed(BUTTON_S)) {
      v = sub(v, forward);
   }

   if (is_button_pressed(BUTTON_A)) {
      v = add(v, right);
   }

   if (is_button_pressed(BUTTON_D)) {
      v = sub(v, right);
   }

   if (is_button_pressed(BUTTON_SPACE)) {
      v = sub(v, Vector3Scale(up, -1));
   }

   if (is_button_pressed(BUTTON_LEFT_CONTROL)) {
      v = sub(v, up);
   }

   if (is_button_pressed(BUTTON_SHIFT)) {
      speed *= 2;
   }

   v = normalize(v);
   v = mul(v, (speed * time_delta()));

   cam.position = Vector3Add(cam.position, v);

   f64 yoffset = get_mouse_scroll();
   if (yoffset != 0.0) {
      cam.zoom = 1+yoffset;
   }

   if (cam.zoom < 1) {
      cam.zoom = 1;
      yoffset = 0;
   }


   return cam;
}
