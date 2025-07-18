
typedef struct {
   Application app; // must be first
   Shader compute;
} Raymarching_Application;

void raymarching_application_init(Raymarching_Application* app) {
   trace_info("Hiiii\n");
}

void raymarching_application_update(Raymarching_Application* app, f64 dt) {
   trace_info("Hiiii\n");
}

Raymarching_Application raymarching_application = {
   .app = create_application(raymarching_application_init, raymarching_application_update)
};
