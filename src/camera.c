
// Pull out camera to camera file (gameplay folder?)
typedef struct {
   Vector3 position;
   Vector3 rotation; // .x value is radians rotation around x-axis
   f32 zoom;
} Camera;

// TODO: Are we s'posed to have is_button_pressed here?
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

   f32 speed = 4 * 20.20f;
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
      speed *= 2.5;
   }

   if (is_button_pressed(BUTTON_C)) {
      speed *= 0.20;
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
