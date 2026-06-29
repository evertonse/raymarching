void draw_scene_depth(Projection_Application *app) {
   static bool loaded = false;
   // Scene_Node node = draw_model("res/models/test-material-height-map-stone-curved/source/Scene CSP HP.fbx", 1);
   // Scene_Node node = draw_model("res/models/substance-designer-scifi-panel-sbs-graph/source/ScifiPanel.fbx", 1);
   // Scene_Node node = draw_model("res/models/substance-designer-scifi-panel-sbs-graph/source/ScifiPanel_copy.fbx", 1);

   if (!loaded) {
      loaded = true;

      Model box_model = create_cube_model(nullptr, nullptr, nullptr, nullptr);
      assert(1 == box_model.materials.count);
      box_model.materials.items[0].diffuse = "res/textures/brickwall.jpg";
      box_model.materials.items[0].normal  = "res/textures/tileable/Cone_Map_1k_normals.png";
      box_model.materials.items[0].height  = "res/textures/tileable/Cone_Map_1k_depth.png";
      box_model.materials.items[0].height  = "res/textures/tileable/Cone_Map_256_depth.png";

      if (true) {
         auto in_cone = box_model.materials.items[0].height;
         auto out_cone = "replace-my-name.conemap.png";
         if (true || !file_exists(out_cone)) {
            generate_cone_map_relaxed(in_cone, &out_cone, nullptr, nullptr);
            // generate_cone_map_relaxed_slower(in_cone, out_cone, nullptr, nullptr);
         }
         box_model.materials.items[0].height = out_cone;
      }
      // box_model.materials.items[0].height = "res/textures/cones/tile1_relaxedcone.tga";
      // box_model.materials.items[0].height  = "res/textures/tileable/conemap_1024.png";
      // box_model.materials.items[0].height  = "res/textures/tileable/Cone_Map_1k_depth.conemap.png";
      // box_model.materials.items[0].height  = "res/textures/tileable/conemap_256.png";


      trace_info("Using this height map: `%s`", box_model.materials.items[0].height);
      {
         auto node = create_scene_node(&box_model);
         update_position(node, vector3(20, 20, 0));
         update_scale(node, 125);
      }
   }

}

