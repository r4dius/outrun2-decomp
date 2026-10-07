#!/usr/bin/env python3
"""Hash-pinned differential test of reconstructed PC driving functions.

This launcher DOES execute selected original x86 arithmetic in a child process.
It never boots the game, patches DRM, or executes the supplied Steam-packed EXE.
Linux x86-64 only; compile with OR2_BUILD_PC_ORACLE=ON. Python standard library.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys

EXPECTED_SHA256 = 'bdafa88a5abdd2a9743f6bdcc5e2189288c0203790412fce10b9bb649933482f'
EXPECTED_SIZE = 14_950_400
STEAM_SHA256 = '7c4e8ec6fcc54e04bfd82e6165e789dfe462e898dcde6f1107d7cc72799fa93d'   # unpacked Steam build (the base)
STEAM_SIZE = 5_828_096
FRONTEND_WELCOME_PROBE = 'frontend_welcome_probe'
FRONTEND_LICENSE_PROBE = 'frontend_license_probe'
FRONTEND_SPRITES_PROBE = 'frontend_sprites_probe'
FRONTEND_TEXT_PROBE = 'frontend_text_probe'
FRONTEND_KEYBOARD_PROBE = 'frontend_keyboard_probe'
FRONTEND_GLYPH_RENDER_PROBE = 'frontend_glyph_render_probe'
FRONTEND_IMAGE_RENDER_PROBE = 'frontend_image_render_probe'
FRONTEND_LICENSE_PANELS_PROBE = 'frontend_license_panels_probe'
FRONTEND_LIST_PROBE = 'frontend_list_probe'
FRONTEND_COMMANDS_PROBE = 'frontend_commands_probe'
FRONTEND_INPUT_PROBE = 'frontend_input_probe'
FRONTEND_FIXED_CHOICE_PROBE = 'frontend_fixed_choice_probe'
FRONTEND_RECORD_MANAGER_PROBE = 'frontend_record_manager_probe'
FRONTEND_VEHICLE_LOADER_PROBE = 'frontend_vehicle_loader_probe'
FRONTEND_VEHICLE_MENU_PROBE = 'frontend_vehicle_menu_probe'
FRONTEND_VEHICLE_PREVIEW_PROBE = 'frontend_vehicle_preview_probe'
VEHICLE_CONSTRUCTOR_PROBE = 'vehicle_constructor_probe'
VEHICLE_BODY_PROBE = 'vehicle_body_probe'
ENVIRONMENT_BLEND_PROBE = 'environment_blend_probe'
VEHICLE_PREVIEW_INIT_PROBE = 'vehicle_preview_init_probe'
CAMERA_PROBE = 'camera_probe'
VEHICLE_MODEL_DRAW_PROBE = 'vehicle_model_draw_probe'
RACE_MODEL_DRAW_PROBE = 'race_model_draw_probe'
RACE_EVENTS_PROBE = 'race_events_probe'
RACE_AREA_PROBE = 'race_area_probe'
RACE_SKY_PROBE = 'race_sky_probe'
RENDERER_INIT_PROBE = 'renderer_init_probe'
COURSE_OBJECTS_PROBE = 'course_objects_probe'
SPRITE_2D_FLUSH_PROBE = 'sprite_2d_flush_probe'
SPRITE_2D_SPRANI_PROBE = 'sprite_2d_sprani_probe'
SPRITE_2D_POOL_PROBE = 'sprite_2d_pool_probe'
SPRITE_2D_IMAGE_PROBE = 'sprite_2d_image_probe'
ROB_MOTION_PROBE = 'rob_motion_probe'
ROB_SETBONE_PROBE = 'rob_setbone_probe'
ROB_DISP_PROBE = 'rob_disp_probe'
OBJECT_DB_PROBE = 'object_db_probe'
ROB_OSAGE_PROBE = 'rob_osage_probe'
ROB_MOTION_ENGINE_PROBE = 'rob_motion_engine_probe'
CHAR_GRIP_PROBE = 'char_grip_probe'
ROB_DISPLAY_PROBE = 'rob_display_probe'
ROB_FLAG_PROBE = 'rob_flag_probe'
ROB_FLAG_DRAW_PROBE = 'rob_flag_draw_probe'
SHADOW_VOLUME_PROBE = 'shadow_volume_probe'
SCENE_EFFECTS_PROBE = 'scene_effects_probe'   # scn-efc
PARTICLES_PROBE = 'particles_probe'   # scn-efc
RACE_ROBOTS_PROBE = 'race_robots_probe'
RENDER_QUEUE_PROBE = 'render_queue_probe'
PMT_LOADER_PROBE = 'pmt_loader_probe'
RENDER_FLUSH_PROBE = 'render_flush_probe'
SCENE_DISPLAY_PROBE = 'scene_display_probe'
ENVIRONMENT_RENDER_PROBE = 'environment_render_probe'
FRAME_RENDER_PROBE = 'frame_render_probe'
SCENE_ENVIRONMENT_PROBE = 'scene_environment_probe'
TRANSMISSION_PROBE = 'frontend_transmission_probe'
MUSIC_PROBE = 'frontend_music_probe'
RANKINGS_PROBE = 'frontend_rankings_probe'
GHOST_START_PROBE = 'ghost_start_probe'
GHOST_CAR_PROBE = 'ghost_car_probe'
GHOST_DRAW_PROBE = 'ghost_draw_probe'
RACE_END_PROBE = 'race_end_probe'
GOAL_CAMERA_PROBE = 'goal_camera_probe'
GHOSTS_PROBE = 'frontend_ghosts_probe'
COURSES_PROBE = 'frontend_courses_probe'
MISSION_MANAGER_PROBE = 'mission_manager_probe'
RACE_HUD_PROBES = ('race_hud_sprani_probe', 'race_hud_gad_pub_probe', 'race_hud_mission_probe', 'race_hud_navi_probe')
RACER_SETUP_PROBE = 'racer_setup_probe'
SCENE_OWNER_PROBE = 'scene_owner_probe'
RACE_MANAGER_PROBE = 'race_manager_probe'
RACE_SOUND_PROBE = 'race_sound_probe'  # race SOUND (event 383) / COMM_TRANS (360)
TITLE_MENU_CONTROL_PROBE = 'title_menu_control_probe'
TITLE_OPTIONS_PROBE = 'title_options_probe'
ROUTINES = ('all', 'friction', 'clutch', 'spin', 'grip', 'torque', 'predicted', 'accel', 'brake', 'cornering', 'side', 'front_force', 'rear_force', 'circle', 'resolve', 'front_rotation', 'rear_rotation', 'rolling', 'slip', 'brake_distribution', 'steering', 'toe', 'direction_angles', 'running', 'copy_physical', 'tire_velocity', 'tire_direction', 'auto_transmission', 'manual_transmission', 'road_mu', 'driving_control', 'specific_tire_load', 'diagonal_tire_load', 'suspension_force', 'tire_load', 'contact_matrix', 'maximum_velocity', 'calc_collision_area', 'course_triangle_plane_y', 'course_quad_plane_y', 'update_easy_lct_prediction', 'course_collision_offset_direction', 'course_length', 'course_collision_ext_flags', 'get_y_position_spl_chk', 'car_sus_coli_check', 'car_sus_bump_push', 'collision_suspension_chain', 'course_spline_all', 'course_vec3_distance', 'course_vec3_length_squared', 'calc_normal_to_delta', 'calc_hermite_direction_vector2', 'calc_hermite_tangent', 'calc_hermite_tangent2', 'calc_hermite_coefficients', 'calc_carry_variable', 'solve_hermite', 'calc_hermite2', 'calc_y_pos_spl', 'course_spline_chain', 'find_primary_course_run', 'course_query_all', 'find_secondary_course_run', 'coli_get_forward_polygon_number', 'coli_get_back_polygon_number', 'coli_get_left_polygon_number', 'coli_get_right_polygon_number', 'get_road_cond', 'course_query_chain', 'course_world_all', 'matrix_push', 'matrix_pop', 'matrix_load', 'matrix_inverse_point', 'matrix_point', 'get_y_position_prog', 'get_y_position_spl_chk_closed', 'course_world_chain', 'matrix_push_load', 'matrix_vector', 'course_collision_world_normal', 'get_cs_road_info_by_cs_len', 'pl_wrecker_sub', 'advance_on_road_place', 'pl_wrecker_delayed', 'wrecker_r022_all', 'matrix_push_unit', 'matrix_unit_rotation', 'matrix_get', 'matrix_primitives_r023_all', 'ground_all', 'calc_ground_coli_face', 'ground_suspension_chain')
ROUTINES += ('start_fight_vm_probe', 'course_field_vm_probe', 'title_owner_vm_probe', 'title_owner_dispatch_probe', 'title_owner_constructor_probe', 'title_owner_children_probe', 'title_owner_full_constructor_probe', 'title_owner_controller_init_probe', 'title_owner_tick_dispatch_probe')
ROUTINES += ('matrix_rotate_x', 'matrix_rotate_z', 'matrix_translate_vector', 'posture_primitives_r024_all', 'reconstruct_posture_matrix_and_face_work', 'calc_disp_matrix', 'pl_wrecker_immediate', 'crash_entry_wrecker_chain_r025')
ROUTINES += ('wall_geometry_all', 'matrix_load_rotation', 'matrix_inverse_vector', 'cop_coli_point', 'calc_coli_wall_face', 'push_outpos_mat_obsolete', 'coli_set_obsolete', 'wall_geometry_chain')
D3DX29_SIZE = 2_332_368
ROUTINES += RACE_HUD_PROBES
ROUTINES += (FRONTEND_FIXED_CHOICE_PROBE,)
ROUTINES += (FRONTEND_RECORD_MANAGER_PROBE,)
ROUTINES += (FRONTEND_VEHICLE_LOADER_PROBE,)
ROUTINES += (FRONTEND_VEHICLE_MENU_PROBE,)
ROUTINES += (FRONTEND_VEHICLE_PREVIEW_PROBE,)
ROUTINES += (VEHICLE_CONSTRUCTOR_PROBE,)
ROUTINES += (VEHICLE_BODY_PROBE,)
ROUTINES += (ENVIRONMENT_BLEND_PROBE,)
ROUTINES += (VEHICLE_PREVIEW_INIT_PROBE,)
ROUTINES += (CAMERA_PROBE,)
ROUTINES += (VEHICLE_MODEL_DRAW_PROBE,)
ROUTINES += (RACE_MODEL_DRAW_PROBE,)
ROUTINES += (RACE_EVENTS_PROBE,)
ROUTINES += (RACE_AREA_PROBE, RACE_SKY_PROBE)
ROUTINES += (RENDERER_INIT_PROBE, COURSE_OBJECTS_PROBE, SPRITE_2D_FLUSH_PROBE, SPRITE_2D_SPRANI_PROBE, SPRITE_2D_POOL_PROBE, SPRITE_2D_IMAGE_PROBE, ROB_MOTION_PROBE, ROB_SETBONE_PROBE, ROB_DISP_PROBE, OBJECT_DB_PROBE, ROB_OSAGE_PROBE, ROB_MOTION_ENGINE_PROBE, CHAR_GRIP_PROBE, ROB_DISPLAY_PROBE, ROB_FLAG_PROBE, ROB_FLAG_DRAW_PROBE, SHADOW_VOLUME_PROBE, SPRITE_2D_SPRANI_PROBE)
ROUTINES += (SCENE_EFFECTS_PROBE, PARTICLES_PROBE)   # scn-efc
ROUTINES += (RACE_ROBOTS_PROBE,)
ROUTINES += (RENDER_QUEUE_PROBE,)
ROUTINES += (PMT_LOADER_PROBE,)
ROUTINES += (RENDER_FLUSH_PROBE,)
ROUTINES += (SCENE_DISPLAY_PROBE, ENVIRONMENT_RENDER_PROBE, FRAME_RENDER_PROBE, SCENE_ENVIRONMENT_PROBE, TRANSMISSION_PROBE, MUSIC_PROBE, MISSION_MANAGER_PROBE, RACER_SETUP_PROBE, SCENE_OWNER_PROBE, RACE_MANAGER_PROBE)
ROUTINES += (RACE_SOUND_PROBE,)
ROUTINES += (RANKINGS_PROBE, GHOSTS_PROBE, COURSES_PROBE, GHOST_START_PROBE, GHOST_CAR_PROBE, GHOST_DRAW_PROBE, RACE_END_PROBE, GOAL_CAMERA_PROBE)
D3DX29_SHA256 = 'c5e21c18f8c79bc517da59e3192c39ea73bdcaf85867628187f6b3cca07dd21f'
D3DX_ROUTINES = {'all', 'tire_velocity', 'tire_direction', 'driving_control', 'tire_load', 'contact_matrix', 'maximum_velocity', 'car_sus_coli_check', 'car_sus_bump_push', 'collision_suspension_chain', 'course_world_all', 'matrix_push', 'matrix_pop', 'matrix_load', 'matrix_inverse_point', 'matrix_point', 'get_y_position_prog', 'get_y_position_spl_chk_closed', 'course_world_chain', 'matrix_push_load', 'matrix_vector', 'course_collision_world_normal', 'get_cs_road_info_by_cs_len', 'pl_wrecker_sub', 'advance_on_road_place', 'pl_wrecker_delayed', 'wrecker_r022_all', 'matrix_push_unit', 'matrix_unit_rotation', 'matrix_get', 'matrix_primitives_r023_all', 'ground_all', 'calc_ground_coli_face', 'ground_suspension_chain'}
D3DX_ROUTINES.add(RACE_SOUND_PROBE)
D3DX_ROUTINES.add(GHOST_CAR_PROBE)
D3DX_ROUTINES.add(GHOST_DRAW_PROBE)
D3DX_ROUTINES.add(RACE_END_PROBE)
D3DX_ROUTINES.add(GOAL_CAMERA_PROBE)
D3DX_ROUTINES.add(RANKINGS_PROBE)  # 51C160: D3DXMatrixTranslation (4393A6)
D3DX_ROUTINES.add(GHOSTS_PROBE)
D3DX_ROUTINES.add(COURSES_PROBE)  # race SOUND: D3DXVec3Normalize (4393E8)

D3DX_ROUTINES.update(('matrix_push_unit', 'matrix_unit_rotation', 'matrix_get', 'matrix_primitives_r023_all'))
D3DX_ROUTINES.update(('matrix_rotate_x', 'matrix_rotate_z', 'matrix_translate_vector', 'posture_primitives_r024_all', 'reconstruct_posture_matrix_and_face_work', 'calc_disp_matrix', 'pl_wrecker_immediate', 'crash_entry_wrecker_chain_r025'))

D3DX_ROUTINES.update(['wall_geometry_all', 'matrix_load_rotation', 'matrix_inverse_vector', 'cop_coli_point', 'calc_coli_wall_face', 'push_outpos_mat_obsolete', 'coli_set_obsolete', 'wall_geometry_chain'])

ROUTINES += ('wall_response_all', 'stage_find_record', 'stage_number', 'check_crush_entrapment_length', 'check_friction_entrapment_length', 'collision_timer', 'matrix_identity', 'matrix_store_rotation', 'matrix_multiply_current', 'matrix_rotate_y', 'calc_friction_status', 'calc_rebound_heading', 'wall_response_chain')
D3DX_ROUTINES.update(('wall_response_all', 'stage_find_record', 'stage_number', 'check_crush_entrapment_length', 'check_friction_entrapment_length', 'collision_timer', 'matrix_identity', 'matrix_store_rotation', 'matrix_multiply_current', 'matrix_rotate_y', 'calc_friction_status', 'calc_rebound_heading', 'wall_response_chain'))

ROUTINES += ('wall_rebound_all', 'stage_property', 'pack_route', 'unpack_route', 'save_route_progress', 'set_route_choice', 'get_route_choice', 'enqueue_sound', 'calc_rebound_heading_float', 'direction_xz', 'cw_rebound_status', 'wall_rebound_chain', 'wall_friction_rebound_chain')
D3DX_ROUTINES.update(('wall_rebound_all', 'stage_property', 'pack_route', 'unpack_route', 'save_route_progress', 'set_route_choice', 'get_route_choice', 'enqueue_sound', 'calc_rebound_heading_float', 'direction_xz', 'cw_rebound_status', 'wall_rebound_chain', 'wall_friction_rebound_chain'))

ROUTINES += ('crash_all', 'wall_impact_angle', 'collision_material_sound', 'course_end_position', 'crash_effect_dispatch', 'crash_key_interpolation', 'crash_segment_search', 'crash_channel_sample', 'crash_pose_sample', 'crash_duration', 'crash_recovery_duration', 'advance_crash_state', 'crash_state_chain', 'collision_material_sound_chain')
D3DX_ROUTINES.update(('crash_all', 'wall_impact_angle', 'collision_material_sound', 'course_end_position', 'crash_effect_dispatch', 'crash_key_interpolation', 'crash_segment_search', 'crash_channel_sample', 'crash_pose_sample', 'crash_duration', 'crash_recovery_duration', 'advance_crash_state', 'crash_state_chain', 'collision_material_sound_chain'))


# r026: complete CwCrushStatus parent through active StartCrash -> PlWrecker.
ROUTINES += ('cw_crush_status_r026', 'cbw_coli_wall_r027', 'get_y_position_prog_bk_r028', 'car_body_wall_coli_check_r028', 'coli_car_r028', 'action_force2_r029', 'matrix_rotate_axis_r029', 'set_force_obsolete_r029', 'make_force_work_sus_r029')
D3DX_ROUTINES.update(('cw_crush_status_r026', 'cbw_coli_wall_r027', 'get_y_position_prog_bk_r028', 'car_body_wall_coli_check_r028', 'coli_car_r028', 'action_force2_r029', 'matrix_rotate_axis_r029', 'set_force_obsolete_r029', 'make_force_work_sus_r029'))

ROUTINES += ('calc_game_resist_r030', 'force_work_49fed0_r030', 'ass_cancel_incline_resistance_r030', 'ass_press_down_against_road_r030', 'make_force_work_r030')
D3DX_ROUTINES.update(('calc_game_resist_r030', 'force_work_49fed0_r030', 'ass_cancel_incline_resistance_r030', 'ass_press_down_against_road_r030', 'make_force_work_r030'))

ROUTINES += ('rear_grip_curve_r031', 'rear_grip_ctrl_r031', 'slip_angle_ctrl_r031', 'cornering_ctrl_r031', 'calc_pl_body_force_limit_r031')
D3DX_ROUTINES.update(('rear_grip_curve_r031', 'rear_grip_ctrl_r031', 'slip_angle_ctrl_r031', 'cornering_ctrl_r031', 'calc_pl_body_force_limit_r031'))

ROUTINES += ('make_force_work_tire_4a0000_r032', 'assist_wanderer_4a5470_r032', 'reb_pl_body_base_4a0200_r032')
D3DX_ROUTINES.update(('make_force_work_tire_4a0000_r032', 'assist_wanderer_4a5470_r032', 'reb_pl_body_base_4a0200_r032'))

ROUTINES += ('reb_pl_body_sub3_4a08f0_r033', 'reb_pl_body_sub2_4a0320_r033', 'reb_pl_body_4a62e0_r033', 'wrec_pl_body_4a0c70_r034', 'ass_compulsive_move_5184b0_r035', 'calc_pl_body_2nd_4a7ec0_r035')
ROUTINES += ('check_night_and_tunnel_4a25f0_r036', 'calc_disp_steering_angle_46eb40_r036')
ROUTINES += ('copy_car_work_4a1140_r037', 'set_car_camera_4a1680_r038', 'check_driving_skill_4a4830_r038', 'check_chicken_driver_4a4900_r039', 'handicap_control_458e40_r039', 'assist_chicken_driver_4a4ba0_r040', 'calc_vibrate_matrix_4a2d70_r040', 'check_reverse_car_4a2910_r041', 'calc_ofs_left_lane_4a45f0_r041', 'calc_light_rate_4a3d40_r042', 'record_ghost_car_4a4710_r042')
ROUTINES += ('session_mode4_4962a0_r043', 'session_mode6_4962d0_r043', 'othcar_calc_r_46f040_r043', 'othcar_calc_r_alt_46f190_r043', 'othcar_get_r_479670_r043')
ROUTINES += ('r043_postghost_batch',)
ROUTINES += ('platform_flag_bit2_44ff10_r044','platform_index_value_4505a0_r044','platform_pair_value_450630_r044','platform_slot_kind_450750_r044','platform_counter_lt_60_48b350_r044','platform_ghost_route_4671d0_r044','platform_ghost_record_47f780_r044','common_pl_car_inline_tail_4a82c4_r044','r044_platform_batch')
ROUTINES += ('course_nested_marker_44bdb0_r045','course_stage_limit_44be00_r045','course_active_slot_44be10_r045','course_primary_ready_44be30_r045','course_secondary_ready_44be40_r045','course_type_gate_44be50_r045','course_index_lookup_44be80_r045','course_disp_matrix_choice_44bed0_r045','course_area_matrix_choice_44bef0_r045','course_clear_service_state_44bf10_r045','course_mark_ready_44c080_r045','course_get_mode_byte_44c090_r045','course_set_mode_byte_44c0a0_r045','course_copy_snapshot_44c0b0_r045','r045_course_services_batch')
ROUTINES += ('course_stage_unique_44dc50_r046','road_stage_window_44ddc0_r046','road_stage_gate_44f0f0_r046','road_decode_sample_46ffc0_r046','road_table_choice_4700d0_r046','road_side_test_479a70_r046','road_lane_classify_47b890_r046','refresh_cached_road_4a3f80_r046','get_road_ofs_4a4010_r046','common_pl_car_4a8100_r046','r046_road_services_batch')
ROUTINES += ('game_flag_43f9c0_r047','set_game_flag_43f9d0_r047','set_game_state_byte_43f9e0_r047','game_state_byte_43f9f0_r047','game_state_dword_43fa00_r047','set_game_state_dword_43fa10_r047','game_broadcast_43cc20_r047','operation_input_49fad0_r047','check_shift_warning_4a50f0_r047','car_calc_total_cs_len_455f50_r047','car_calc_current_stage_progress_4a2130_r047','get_now_heart_calc_mode_45c440_r047','set_old_param_buffer_4a2ee0_r047','r047_game_control_batch')
ROUTINES += ('race_counter_44fdf0_r048','set_timeup_counter_44fe30_r048','get_timeup_counter_44fe40_r048','set_race_flag0_44fe50_r048','get_race_flag0_44fe70_r048','set_race_flag2_44fef0_r048','ham_nos_speed_45d0e0_r048','game_pl_car_ctrl_4a8330_r048','r048_game_parent_batch')
ROUTINES += ('control_timeup_braking_49fb70_r049','check_wanderer_4a5260_r049','ham_nos_set_speed_4a5650_r049','network_tail_state_55a930_r049','network_tail_forward_46c390_r049','rank_provider_gateway_45a2b0_r049','r049_game_children_batch')
ROUTINES += ('check_slipstream_4a4d20_r050','r050_game_children_batch')
ROUTINES += ('pas_pl_car_ctrl_475720_r051','r051_pas_parent_batch')
ROUTINES += ('event_control_43fab0_r052','event_open_440180_r052','event_close_4401d0_r052','event_close_immediate_440200_r052','event_close_all_440240_r052','event_close_serial_440330_r052','check_event_destructing_440370_r052','get_event_id_440b30_r052','get_now_event_id_440b80_r052','change_now_event_ctrl_func_440b90_r052','change_now_event_shadow_func_440ba0_r052','change_ctrl_func_440bb0_r052','change_disp_scene_440bd0_r052','r052_event_scheduler_batch')
ROUTINES += ('init_event_control_440bf0_r053','event_suspend_440a10_r053','event_resume_440a30_r053','check_event_suspend_440a50_r053','r053_event_bootstrap_batch')
ROUTINES += ('set_ev_pause_flag_440930_r054','clr_ev_pause_flag_4409c0_r054','malloc_now_event_work_440a60_r054','free_now_event_work_440b20_r054','free_event_work_handle_440cd0_r054','r054_event_work_batch')
ROUTINES += ('push_alloc_state_a_440d10_r055','pop_alloc_state_a_440d30_r055','push_alloc_state_b_440d50_r055','pop_alloc_state_b_440d70_r055','hmm_handle_move_440cc0_r055','masked_service_440ca0_r055','r055_allocator_state_batch')
ROUTINES += ('object_take_token_440dc0_r056','object_set_token_440de0_r056','object_set_field4_440df0_r056','object_set_field8_440e00_r056','object_push_byte_state_440e10_r056','object_pop_byte_state_440e60_r056','object_child_call_a_440ea0_r056','object_child_call_b_440eb0_r056','object_child_call_c_440ec0_r056','object_child_conditional_440ed0_r056','r056_object_state_batch')
ROUTINES += ('transition_global_get_active_4c50c0_r057','transition_global_set_token_4c50e0_r057','transition_global_set_primary_4c50f0_r057','transition_global_set_secondary_4c5100_r057','transition_global_clear_active_4c5110_r057','object_transition_init_440ef0_r057','object_transition_config_440f70_r057','object_transition_update_441020_r057','object_transition_latch_441130_r057','r057_transition_batch')
ROUTINES += ('runtime_shutdown_4411a0_r058','runtime_release_handle_441200_r058','runtime_ready_441260_r058','runtime_status_4412c0_r058','runtime_has_handle_4412f0_r058','runtime_close_if_status_441300_r058','runtime_manager_reset_454200_r058','runtime_set_gate_454220_r058','runtime_get_gate_454230_r058','runtime_finish_454240_r058','runtime_child_counter_dec_4464f0_r058','runtime_child_state_564c90_r058','r058_runtime_batch')
ROUTINES += ('ui_notify_441370_r059','ui_set_active_4413f0_r059','compact_u32_list_441410_r059','ui_resource_reset_465250_r059','ui_resource_ready_4652e0_r059','ui_text_set_xy_42cc00_r059','ui_text_set_color_42cca0_r059','ui_text_set_mode_42ccb0_r059','ui_table_lookup_465eb0_r059','r059_ui_batch')
ROUTINES += ('object_factory_441450_r060','object_factory_4414b0_r060','object_factory_441510_r060','object_factory_441570_r060','object_factory_4415d0_r060','object_factory_441630_r060','object_factory_441690_r060','object_factory_4416f0_r060','object_factory_441750_r060','object_factory_4417b0_r060','object_factory_441810_r060','object_factory_441870_r060','object_factory_4418d0_r060','r060_factory_batch')
ROUTINES += ('object_factory_441930_r061','object_factory_441990_r061','object_factory_4419f0_r061','object_factory_441a50_r061','object_factory_441ab0_r061','object_factory_441b10_r061','object_factory_441b70_r061','object_factory_441bd0_r061','object_factory_441c30_r061','object_factory_441c90_r061','object_factory_441cf0_r061','object_factory_441d50_r061','object_factory_441db0_r061','object_factory_441e10_r061','object_factory_441e70_r061','object_factory_441ed0_r061','object_factory_441f30_r061','object_factory_441f90_r061','object_factory_441ff0_r061','object_factory_442050_r061','object_factory_4420b0_r061','object_factory_442110_r061','object_factory_442170_r061','object_factory_4421d0_r061','object_factory_442230_r061','object_factory_442290_r061','object_factory_4422f0_r061','object_factory_442350_r061','object_factory_4423b0_r061','object_factory_442410_r061','object_factory_442470_r061','object_factory_4424d0_r061','object_factory_442530_r061','object_factory_442590_r061','object_factory_4425f0_r061','object_factory_442650_r061','object_factory_4426b0_r061','object_factory_442710_r061','object_factory_442770_r061','object_factory_4427d0_r061','object_factory_442830_r061','object_factory_442890_r061','object_factory_4428f0_r061','object_factory_442950_r061','r061_factory_batch')
ROUTINES += ('object_ctor_4429b0_r062','object_ctor_442a60_r062','object_block_init_442ac0_r062','object_state_ctor_442b20_r062','r062_constructor_batch')
ROUTINES += ('timer_value_4af500_r063','object_state_reset_442c20_r063','object_select_primary_442cb0_r063','object_select_secondary_442d70_r063','r063_state_selector_batch')
ROUTINES += ('object_dispatch_state_442e00_r064','object_refresh_state_table_442e70_r064','object_query_previous_state_442ec0_r064','object_store_depth_pair_442f20_r064','object_query_last_state_443040_r064','object_has_active_state_443060_r064','r064_object_state_batch')
ROUTINES += ('object_store_state_pair_442f50_r065','object_update_state_442fd0_r065','object_release_flagged_handles_4430b0_r065','r065_state_update_batch')
ROUTINES += ('object_ui_state_update_443110_r066','object_event_dispatch_4432b0_r066','r066_ui_dispatch_batch')
ROUTINES += ('object_dispatch_callback_443420_r067','object_factory_4434c0_r067','object_factory_443520_r067','r067_event_factory_batch')
ROUTINES += ('object_destroy_443580_r068','object_destroy_body_4435a0_r068','object_destroy_443640_r068','object_destroy_body_443660_r068','r068_destructor_batch')
ROUTINES += ('object_runtime_init_4436c0_r069','r069_runtime_init_batch')
ROUTINES += ('object_runtime_teardown_443c30_r070','r070_runtime_teardown_batch')
ROUTINES += ('object_dispatch_callback_443eb0_r071','object_open_callback_443fa0_r071','object_state_code_443ff0_r071','r071_event_open_batch')
ROUTINES += ('object_insert_callback_4440f0_r072','object_reopen_callback_4442a0_r072','object_runtime_commit_444350_r072','r072_runtime_list_batch')
ROUTINES += ('runtime_primary_mode_4eea70_r073','runtime_route_mode_4eead0_r073','runtime_route_config_4eeb50_r073','runtime_prepare_4eec80_r073','callback_key_48f4e0_r073','callback_append_48d870_r073','r073_runtime_prepare_batch')
ROUTINES += ('runtime_select_entry_4958a0_r074','runtime_count_entries_495930_r074','runtime_game_flag_43f870_r074','runtime_selection_disable_4957e0_r074','runtime_external_block_4872e0_r074','runtime_system_handle_active_4999c0_r074','runtime_menu_state_450240_r074','runtime_current_player_47f110_r074','runtime_feature_mask_4536f0_r074','object_runtime_gate_444470_r074','r074_runtime_gate_batch')
ROUTINES += ('runtime_course_load_44da00_r075','embedded_slot_refresh_446bb0_r075','embedded_configure_446f30_r075','r075_runtime_blocker_batch')
ROUTINES += ('runtime_course_shuffle_44bf30_r076','runtime_course_tree_44c850_r076','runtime_course_matrix_44c0d0_r076','runtime_course_force_mode_46c360_r076','runtime_apply_course_data_44d720_r076','r076_course_runtime_batch')
ROUTINES += ('runtime_category_hash_4f1260_r077','runtime_category_records_4f1a90_r077','runtime_category_count_4f1ba0_r077','r077_course_provider_batch')
ROUTINES += ('object_runtime_dispatch_primary_445430_r079','object_runtime_dispatch_secondary_4454b0_r079','object_runtime_owner_445be0_r079','r079_runtime_owner_batch')
ROUTINES += ('ui_resource_tick_4659f0_r080','object_runtime_ui_tick_444840_r080','r080_runtime_ui_batch')
ROUTINES += ('embedded_slot_open_primary_446a80_r081','embedded_slot_open_secondary_446b20_r081','embedded_slots_tick_446cf0_r081','r081_embedded_slots_batch')
ROUTINES += ('object_runtime_transition_444530_r083','r083_runtime_transition_batch')
ROUTINES += ('runtime_pair_idle_434de0_r084','object_runtime_open_4459f0_r084','object_runtime_queue_445a50_r084','r084_runtime_queue_batch')
ROUTINES += ('runtime_owner_entry_49e4a0_r085','r085_callback_entry_batch')
ROUTINES += ('event_setup_440110_r086','runtime_init_entry_49e490_r086','runtime_display_entry_49e4b0_r086','runtime_teardown_alias_443e90_r086','runtime_destroy_entry_49e4c0_r086','r086_event_function36_batch')
ROUTINES += ('runtime_bootstrap_417810_r087','r087_bootstrap_batch')
ROUTINES += ('runtime_startup_owner_4176e0_r088','r088_startup_owner_batch')
ROUTINES += ('runtime_platform_init_417740_r090','r090_platform_init_batch')
ROUTINES += ('runtime_loop_cleanup_417970_r091','r091_loop_cleanup_batch')
ROUTINES += ('runtime_loop_setup_417a20_r092','r092_loop_setup_batch')
ROUTINES += ('runtime_frame_ticks_417890_r093','r093_timing_batch')
ROUTINES += ('runtime_frame_step_417c7b_r094','r094_frame_step_batch')
D3DX_ROUTINES.update(('runtime_course_matrix_44c0d0_r076','runtime_apply_course_data_44d720_r076','r076_course_runtime_batch'))
D3DX_ROUTINES.update(('check_wanderer_4a5260_r049','r049_game_children_batch'))
D3DX_ROUTINES.update(('check_night_and_tunnel_4a25f0_r036', 'calc_disp_steering_angle_46eb40_r036'))
D3DX_ROUTINES.update(('copy_car_work_4a1140_r037', 'set_car_camera_4a1680_r038', 'check_reverse_car_4a2910_r041', 'calc_light_rate_4a3d40_r042'))
D3DX_ROUTINES.update(('get_road_ofs_4a4010_r046','r046_road_services_batch'))
D3DX_ROUTINES.update(('reb_pl_body_sub3_4a08f0_r033', 'reb_pl_body_sub2_4a0320_r033', 'reb_pl_body_4a62e0_r033', 'wrec_pl_body_4a0c70_r034', 'ass_compulsive_move_5184b0_r035', 'calc_pl_body_2nd_4a7ec0_r035'))

ROUTINES += ('crash_entry_all', 'impact_bands', 'impact_feedback_refresh', 'impact_feedback_add', 'crash_entry_candidate', 'crash_start_candidate', 'cw_crush_candidate', 'impact_feedback_chain', 'crash_entry_update_chain', 'cw_crush_update_chain')
D3DX_ROUTINES.update(ROUTINES[-10:])
# Car-control leaf services (pc_car_services): verification and protected-piece measurement.
CAR_SERVICES_PROBE = 'car_services_probe'
CAR_SERVICES_MEASURE = 'car_services_measure'
CAR_SERVICES_GHOST_PROBE = 'car_services_ghost_probe'
CAR_SERVICES_NET_PROBE = 'car_services_net_probe'
CAR_SERVICES_NET_E2E_PROBE = 'car_services_net_e2e_probe'
ROUTINES += (CAR_SERVICES_PROBE, CAR_SERVICES_MEASURE, CAR_SERVICES_GHOST_PROBE, CAR_SERVICES_NET_PROBE, CAR_SERVICES_NET_E2E_PROBE)
D3DX_ROUTINES.update((CAR_SERVICES_PROBE, CAR_SERVICES_GHOST_PROBE, CAR_SERVICES_NET_E2E_PROBE))

def verify_input(path: Path) -> str:
    # The base is the unpacked Steam build (STEAM_SHA256); the FXT EXE (EXPECTED_*)
    # is the same build with protection gateways and is still accepted.
    size = path.stat().st_size
    if size not in (EXPECTED_SIZE, STEAM_SIZE):
        raise ValueError('Wrong EXE size: this oracle requires the unpacked Steam build (5,828,096 bytes) '
                         'or the FXT EXE (14,950,400 bytes). The packed Steam EXE is NOT supported.')
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    digest = h.hexdigest()
    if digest not in (EXPECTED_SHA256, STEAM_SHA256):
        raise ValueError(f'EXE SHA256 mismatch: {digest}; refusing to execute original code')
    return digest

def verify_d3dx29(path: Path) -> str:
    if path.stat().st_size != D3DX29_SIZE:
        raise ValueError(f'Wrong d3dx9_29.dll size: expected {D3DX29_SIZE}')
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != D3DX29_SHA256:
        raise ValueError(f'd3dx9_29.dll SHA256 mismatch: {digest}; refusing to map unpinned helper code')
    return digest

ROUTINES += (FRONTEND_WELCOME_PROBE, FRONTEND_LICENSE_PROBE, FRONTEND_SPRITES_PROBE, FRONTEND_TEXT_PROBE, FRONTEND_KEYBOARD_PROBE, FRONTEND_GLYPH_RENDER_PROBE, FRONTEND_IMAGE_RENDER_PROBE, FRONTEND_LICENSE_PANELS_PROBE, FRONTEND_LIST_PROBE, FRONTEND_COMMANDS_PROBE, FRONTEND_INPUT_PROBE, TITLE_MENU_CONTROL_PROBE, TITLE_OPTIONS_PROBE)

def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', type=Path)
    p.add_argument('--runner', type=Path, required=True)
    p.add_argument('--cases', type=int, default=20000)
    p.add_argument('--routine', choices=ROUTINES, default='all')
    p.add_argument('--x87-control', choices=('0x037f','0x027f','0x007f'), default='0x037f')
    p.add_argument('--timeout', type=float, default=120.0)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--snapshot-out', type=Path, help='Optional original-x86 wheel snapshots, no commercial tables')
    p.add_argument('--inject-mismatch', action='store_true', help='Detector self-test: deliberately corrupt one native expected byte')
    p.add_argument('--d3dx29', type=Path, help='Pinned d3dx9_29.dll from the supplied PC installation; required by tire vector/DrivingControl oracles')
    p.add_argument('--spline-snapshot-out', type=Path, help='Synthetic original-x86 course spline golden records (first 128 cases)')
    p.add_argument('--course-query-snapshot-out', type=Path, help='Synthetic original-x86 course-query golden records (first 256 cases)')
    p.add_argument('--world-snapshot-out', type=Path, help='Original-x86 matrix/world-query golden records (first 256 cases)')
    p.add_argument('--ground-snapshot-out', type=Path, help='Original-x86 ground + retained five-stage sequences')
    p.add_argument("--wall-snapshot-out", type=Path, help="Original-x86 wall geometry and retained sequences")
    p.add_argument("--wall-response-snapshot-out", type=Path, help="Original-x86 wall response + retained calls")
    p.add_argument("--crash-snapshot-out", type=Path, help="Original crash state/keyframes and retained calls")
    p.add_argument("--wall-rebound-snapshot-out", type=Path, help="Original x86 rebound and route/audio snapshots")
    p.add_argument("--crash-entry-snapshot-out", type=Path, help="Original x86 feedback and accepted crash-entry branches; no PlWrecker substitution")
    args = p.parse_args()
    try:
        if platform.system() != 'Linux' or platform.machine().lower() not in ('x86_64','amd64'):
            raise ValueError('The original-code oracle is Linux x86-64 only; portable host tests work separately.')
        if not 1 <= args.cases <= 2_000_000 or not 0 < args.timeout <= 3600:
            raise ValueError('Invalid case count or timeout')
        exe, runner, output = args.exe.resolve(strict=True), args.runner.resolve(strict=True), args.out.resolve()
        if output in (exe, runner):
            raise ValueError('Refusing to overwrite an input with the report')
        snapshot = args.snapshot_out.resolve() if args.snapshot_out else None
        if snapshot and snapshot in (exe, runner, output):
            raise ValueError('Snapshot output must differ from all other inputs and outputs')
        if snapshot:
            snapshot.parent.mkdir(parents=True, exist_ok=True)
        spline_snapshot = args.spline_snapshot_out.resolve() if args.spline_snapshot_out else None
        if spline_snapshot:
            if spline_snapshot in (exe, runner, output, snapshot):
                raise ValueError('Spline snapshot output must differ from every input/output')
            if args.routine not in ('course_spline_all',):
                raise ValueError('Spline snapshots require --routine course_spline_all')
            spline_snapshot.parent.mkdir(parents=True, exist_ok=True)
        query_snapshot = args.course_query_snapshot_out.resolve() if args.course_query_snapshot_out else None
        if query_snapshot:
            if query_snapshot in (exe, runner, output, snapshot, spline_snapshot):
                raise ValueError('Query snapshot output must differ from every input/output')
            if args.routine != 'course_query_all':
                raise ValueError('Query snapshots require --routine course_query_all')
            query_snapshot.parent.mkdir(parents=True, exist_ok=True)
        world_snapshot = args.world_snapshot_out.resolve() if args.world_snapshot_out else None
        if world_snapshot:
            if world_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot):
                raise ValueError('World output must differ from all other inputs and outputs')
            if args.routine != 'course_world_all':
                raise ValueError('World snapshots require --routine course_world_all')
            world_snapshot.parent.mkdir(parents=True, exist_ok=True)
        ground_snapshot = args.ground_snapshot_out.resolve() if args.ground_snapshot_out else None
        if ground_snapshot:
            if ground_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot):
                raise ValueError('Ground snapshot output must differ from every input/output')
            if args.routine != 'ground_all':
                raise ValueError('Ground snapshots require --routine ground_all')
            ground_snapshot.parent.mkdir(parents=True, exist_ok=True)
        wall_snapshot = args.wall_snapshot_out.resolve() if args.wall_snapshot_out else None
        if wall_snapshot:
            if wall_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot):
                raise ValueError('Wall snapshot output must differ from all inputs/outputs')
            if args.routine != 'wall_geometry_all':
                raise ValueError('Wall snapshots require --routine wall_geometry_all')
            wall_snapshot.parent.mkdir(parents=True, exist_ok=True)
        response_snapshot = args.wall_response_snapshot_out.resolve() if args.wall_response_snapshot_out else None
        if response_snapshot:
            if response_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot, wall_snapshot):
                raise ValueError('Response snapshot output must differ from all inputs/outputs')
            if args.routine != 'wall_response_all':
                raise ValueError('Response snapshots require --routine wall_response_all')
            response_snapshot.parent.mkdir(parents=True, exist_ok=True)
        rebound_snapshot = args.wall_rebound_snapshot_out.resolve() if args.wall_rebound_snapshot_out else None
        if rebound_snapshot:
            if rebound_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot, wall_snapshot, response_snapshot):
                raise ValueError('Rebound snapshot conflicts with another input/output')
            if args.routine != 'wall_rebound_all':
                raise ValueError('Rebound snapshots require --routine wall_rebound_all')
            rebound_snapshot.parent.mkdir(parents=True, exist_ok=True)
        crash_snapshot = args.crash_snapshot_out.resolve() if args.crash_snapshot_out else None
        if crash_snapshot:
            if crash_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot, wall_snapshot, response_snapshot, rebound_snapshot):
                raise ValueError('Crash snapshot conflicts with input/output')
            if args.routine != 'crash_all':
                raise ValueError('Crash snapshots require --routine crash_all')
            crash_snapshot.parent.mkdir(parents=True, exist_ok=True)
        entry_snapshot = args.crash_entry_snapshot_out.resolve() if args.crash_entry_snapshot_out else None
        if entry_snapshot:
            if entry_snapshot in (exe, runner, output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot, wall_snapshot, response_snapshot, rebound_snapshot, crash_snapshot):
                raise ValueError('Crash entry snapshot conflicts with input/output')
            if args.routine != 'crash_entry_all':
                raise ValueError('Crash entry snapshots require --routine crash_entry_all')
            entry_snapshot.parent.mkdir(parents=True, exist_ok=True)
        if args.inject_mismatch and args.routine not in RACE_HUD_PROBES + (RENDERER_INIT_PROBE, COURSE_OBJECTS_PROBE, SPRITE_2D_FLUSH_PROBE, SCENE_EFFECTS_PROBE, PARTICLES_PROBE, RACE_MANAGER_PROBE, RACE_SOUND_PROBE, CAR_SERVICES_PROBE, CAR_SERVICES_GHOST_PROBE, CAR_SERVICES_NET_PROBE, CAR_SERVICES_NET_E2E_PROBE, SCENE_OWNER_PROBE, RACER_SETUP_PROBE, MISSION_MANAGER_PROBE, MUSIC_PROBE, RANKINGS_PROBE, GHOSTS_PROBE, COURSES_PROBE, GHOST_START_PROBE, GHOST_CAR_PROBE, GHOST_DRAW_PROBE, RACE_END_PROBE, GOAL_CAMERA_PROBE, TRANSMISSION_PROBE, SCENE_ENVIRONMENT_PROBE, FRAME_RENDER_PROBE, ENVIRONMENT_RENDER_PROBE, SCENE_DISPLAY_PROBE, RENDER_FLUSH_PROBE, PMT_LOADER_PROBE, RENDER_QUEUE_PROBE, VEHICLE_MODEL_DRAW_PROBE, RACE_MODEL_DRAW_PROBE, RACE_EVENTS_PROBE, RACE_AREA_PROBE, RACE_SKY_PROBE, RACE_ROBOTS_PROBE, CAMERA_PROBE, VEHICLE_PREVIEW_INIT_PROBE, ENVIRONMENT_BLEND_PROBE, VEHICLE_BODY_PROBE, VEHICLE_CONSTRUCTOR_PROBE, FRONTEND_VEHICLE_PREVIEW_PROBE, FRONTEND_VEHICLE_MENU_PROBE, FRONTEND_VEHICLE_LOADER_PROBE, FRONTEND_RECORD_MANAGER_PROBE, FRONTEND_FIXED_CHOICE_PROBE, 'all', 'cornering', FRONTEND_WELCOME_PROBE, FRONTEND_LICENSE_PROBE, FRONTEND_SPRITES_PROBE, FRONTEND_TEXT_PROBE, FRONTEND_KEYBOARD_PROBE, FRONTEND_GLYPH_RENDER_PROBE, FRONTEND_IMAGE_RENDER_PROBE, FRONTEND_LICENSE_PANELS_PROBE, FRONTEND_LIST_PROBE, FRONTEND_COMMANDS_PROBE, FRONTEND_INPUT_PROBE, TITLE_MENU_CONTROL_PROBE, TITLE_OPTIONS_PROBE):
            raise ValueError('Mismatch injection requires all, cornering or a supported frontend probe')
        digest = verify_input(exe)
        d3dx_digest = None
        d3dx_path = '-'
        if args.routine in D3DX_ROUTINES or args.routine in RACE_HUD_PROBES or args.routine in (RENDERER_INIT_PROBE, SCENE_EFFECTS_PROBE, PARTICLES_PROBE, VEHICLE_BODY_PROBE, ENVIRONMENT_BLEND_PROBE, VEHICLE_PREVIEW_INIT_PROBE, CAMERA_PROBE, VEHICLE_MODEL_DRAW_PROBE, RACE_MODEL_DRAW_PROBE, RACE_AREA_PROBE, RACE_SKY_PROBE, RACE_ROBOTS_PROBE, RENDER_QUEUE_PROBE, PMT_LOADER_PROBE, RENDER_FLUSH_PROBE, SCENE_DISPLAY_PROBE, ENVIRONMENT_RENDER_PROBE, FRAME_RENDER_PROBE, SCENE_ENVIRONMENT_PROBE):
            if args.d3dx29 is None:
                raise ValueError(f'{args.routine} oracle requires --d3dx29')
            d3dx = args.d3dx29.resolve(strict=True)
            if d3dx in (output, snapshot, spline_snapshot, query_snapshot, world_snapshot, ground_snapshot, wall_snapshot, response_snapshot, rebound_snapshot, crash_snapshot, entry_snapshot):
                raise ValueError('Refusing to overwrite the DirectX input with an output')
            d3dx_digest = verify_d3dx29(d3dx)
            d3dx_path = str(d3dx)
        result = subprocess.run([str(runner), str(exe), str(args.cases), args.routine, args.x87_control,
                                 str(snapshot) if snapshot else '-', 'inject-mismatch' if args.inject_mismatch else '-', d3dx_path, str(spline_snapshot) if spline_snapshot else '-', str(query_snapshot) if query_snapshot else '-', str(world_snapshot) if world_snapshot else '-', str(ground_snapshot) if ground_snapshot else '-', str(wall_snapshot) if wall_snapshot else '-', str(response_snapshot) if response_snapshot else '-', str(rebound_snapshot) if rebound_snapshot else '-', str(crash_snapshot) if crash_snapshot else '-', str(entry_snapshot) if entry_snapshot else '-'],
                                capture_output=True, text=True, timeout=args.timeout, check=False)
        if result.stderr:
            print(result.stderr, file=sys.stderr, end='')
        try:
            report = json.loads(result.stdout)
        except json.JSONDecodeError as exc:
            raise RuntimeError(f'Oracle did not return a valid report (exit {result.returncode}); '
                               'compatibility-mode support/mapping or original-code execution failed') from exc
        report['input_sha256'] = digest
        if d3dx_digest:
            report['d3dx9_29_sha256'] = d3dx_digest
        report['mismatch_injection'] = args.inject_mismatch
        report['wheel_fixture_seed'] = '0x4f523035 XOR (case_index * 1664525)'
        report['wheel_sequences'] = {'sequences': 16 if args.routine == 'all' else 0,
                                    'steps_per_sequence': 512, 'checkpoints_per_step': 17,
                                    'additional_whole_stage_comparison': True}
        if snapshot:
            report['snapshot_file_sha256'] = hashlib.sha256(snapshot.read_bytes()).hexdigest()
        if spline_snapshot:
            report['spline_snapshot_sha256'] = hashlib.sha256(spline_snapshot.read_bytes()).hexdigest()
        if query_snapshot:
            report['course_query_snapshot_sha256'] = hashlib.sha256(query_snapshot.read_bytes()).hexdigest()
        if world_snapshot:
            report['world_snapshot_sha256'] = hashlib.sha256(world_snapshot.read_bytes()).hexdigest()
        if ground_snapshot:
            report['ground_snapshot_sha256'] = hashlib.sha256(ground_snapshot.read_bytes()).hexdigest()
        if wall_snapshot:
            report['wall_snapshot_sha256'] = hashlib.sha256(wall_snapshot.read_bytes()).hexdigest()
        if response_snapshot:
            report['wall_response_snapshot_sha256'] = hashlib.sha256(response_snapshot.read_bytes()).hexdigest()
        if rebound_snapshot:
            report['wall_rebound_snapshot_sha256'] = hashlib.sha256(rebound_snapshot.read_bytes()).hexdigest()
        if crash_snapshot:
            report['crash_snapshot_sha256'] = hashlib.sha256(crash_snapshot.read_bytes()).hexdigest()
        if entry_snapshot:
            report['crash_entry_snapshot_sha256'] = hashlib.sha256(entry_snapshot.read_bytes()).hexdigest()
        report['crash_entry_boundary'] = 'r026: public CwCrushStatus 0x5038D0 is retained through StartCrash 0x4A6EA0 and public PlWrecker 0x504900 on the validated active-tow domain; states 1/2/5, optional 1->5 reroute, immediate/delayed phases, material sound and crash command queue are compared from independent states. Synthetic PC-layout data only, no ARM64 or Switch validation.'
        report['crash_fixture_seed'] = '0x4f523020 XOR (case_index * 1664525)'
        report['crash_boundary'] = 'Original public crash state/keyframe entries and original callees; synthetic keyed curves and retained state. r026 additionally validates complete CwCrushStatus over an active StartCrash/PlWrecker domain; no recorded race, ARM64 or Switch execution.'
        report['wall_rebound_boundary'] = 'Original recoil, route selection/save and sound queue; full arena and relevant globals compared after every retained call. No audio playback, crush dispatch, real track, ARM64 or Switch validation.'
        report['wall_response_boundary'] = 'Eleven closed PC entries and three retained CalcFrictionStatus calls, explicit synthetic stage descriptors and independent per-stage ranges. Original public protected entries, generic x87 D3DX29. Full 20 KiB arena + eight global pages compared after each call without resynchronization. No complete wall dispatcher or Switch validation.'
        report['wall_boundary'] = 'Six original geometric/matrix routines; 8-stage retained wall geometry sequence, explicit PC-layout synthetic tables. No complete CbwColiWall/CarBodyWallColiCheck/ColiCar, no resources or Switch run. Native ColiSet rejects >12 selected negative projections (original local array capacity). No native/oracle resynchronization.'
        report['ground_boundary'] = 'Direct 0x519500 + two retained five-stage sequences: ground, suspension check, bump push, suspension force, tire load. No missing body-wall stage is represented as implemented. Synthetic PC-layout geometry, generic D3DX29, no real track assets or ARM64 result.'
        report['course_world_boundary'] = ('Original public PC entries, two-type world-grid query, closed spline-check wrapper, r025 crash-entry -> PlWrecker chain, and r026 retained CwCrushStatus parent; '
                                           'selected generic x87 D3DX29, synthetic PC tables, explicit matrix stack and prediction. '
                                           'The retained parent chains start original/native from independent world copies and compare owned event/work/body/wheel/contact/audio plus relevant world globals; '
                                           'not a real-track car simulation or ARM64 result.')
        report['course_query_corpus'] = 'Synthetic PC-layout four-type tables; direct original public entries and original internal calls, no replacement neighbor/Hermite callback'
        report['course_spline_fixture_seed'] = '0x4f523014 XOR (case_index * 1664525)'
        report['course_spline_boundary'] = 'Direct original PC spline arithmetic, no replacement callback; synthetic nondegenerate PC-layout polygons, all 16 neighbor masks, explicit original tuning globals'
        report['input_size'] = EXPECTED_SIZE
        report['oracle'] = ('Isolated original x86 protected menu/course entry; not game execution'
                            if args.routine in ('start_fight_vm_probe', 'course_field_vm_probe', 'title_owner_vm_probe', 'title_owner_dispatch_probe', 'title_owner_constructor_probe', 'title_owner_children_probe', 'title_owner_full_constructor_probe', 'title_owner_controller_init_probe', 'title_owner_tick_dispatch_probe')
                            else 'Direct original x86 arithmetic via Linux x86-64 CPU compatibility mode; not game execution')
        report['host'] = {'system': platform.system(), 'machine': platform.machine()}
        report['validation_limit'] = ('Deterministic synthetic state and private original lookup tables/services. '
                                      'DrivingControl is compared as a whole, but there is still no recorded race, '
                                      'Steam equivalence, ARM64 run, full chassis/world integration, or Switch validation.')
        if args.routine in ('start_fight_vm_probe', 'course_field_vm_probe', 'title_owner_vm_probe', 'title_owner_dispatch_probe', 'title_owner_constructor_probe', 'title_owner_children_probe', 'title_owner_full_constructor_probe', 'title_owner_controller_init_probe', 'title_owner_tick_dispatch_probe'):
            report['validation_limit'] = ('The original public x86 entry and protected VM ran against controlled PC-layout state. '
                                          'This proves only the tested input/output mapping; it does not initialize '
                                          'the live menu/course owners, execute a race, or validate the Switch build.')
        if args.routine == 'title_owner_vm_probe':
            report['validation_limit'] = ('Probe only: 4D5D00 and its original VM ran with a synthetic title object. '
                                          'The singleton getter, child virtual callback and pause service were stubbed; '
                                          'the real constructor, child lifecycle and menu are not reproduced.')
        if args.routine == 'title_owner_dispatch_probe':
            report['validation_limit'] = ('The original 4D7260 switch and jump table ran on all states 0-24. '
                                          'Its child callback and five destination handlers were replaced by '
                                          'sentinels, so this validates routing only, not handler behavior or menu play.')
        if args.routine == 'title_owner_constructor_probe':
            report['validation_limit'] = ('The original key-44 parent constructor ran on 32 synthetic objects. '
                                          'Nested constructors were replaced with call-recording sentinels and '
                                          'Windows fs:0 SEH access neutralized; child bodies and UI are not validated.')
        if args.routine == 'title_owner_children_probe':
            report['validation_limit'] = ('Three direct original x86 child constructors were compared on 32 '
                                          'synthetic objects each. This validates their fixed fields only; it '
                                          'does not create the key-44 owner or make the menu playable.')
        if args.routine == 'title_owner_full_constructor_probe':
            report['validation_limit'] = ('The complete original key-44 constructor and nested child constructors '
                                          'ran on 32 synthetic objects. The CRT array constructor was replaced by '
                                          'an exact no-op matching its four 570AC0 element calls, and Windows SEH '
                                          'access was neutralized. This does not validate owner control, UI, or '
                                          'a playable Switch menu.')
        if args.routine == 'title_owner_controller_init_probe':
            report['validation_limit'] = ('Direct original 48C5B0 virtual initializer compared against the native '
                                          'port on 32 synthetic controller objects. This does not validate the '
                                          'title owner control loop or menu behavior.')
        if args.routine == 'title_owner_tick_dispatch_probe':
            report['validation_limit'] = ('The original 4D8C50 key-44 tick and all 24 jump-table states ran on '
                                          'synthetic objects. The child controller and destination handlers were '
                                          'stubbed; this validates dispatch and local fields only, not the menu.')
        if args.routine == FRONTEND_WELCOME_PROBE:
            report['validation_limit'] = ('Original welcome 4C5180/4C5210/4C5350 compared on 388 synthetic states. '
                                          'External input/resource/profile/audio services were stubbed. '
                                          'This checks local state and control returns, not profile persistence or a playable game.')
        if args.routine == FRONTEND_LICENSE_PROBE:
            report['validation_limit'] = ('Original license constructor, unlock, name, selection grids and '
                                          'all eight key-24 parent states compared byte-for-byte. '
                                          'Parent internal helpers run unchanged; graphics, keyboard/text widgets, '
                                          'random and save leaves are stubbed. The protected name/highlight '
                                          'bridges execute unchanged. Does not validate chooser key 21, '
                                          'rendered menus, file persistence or Switch gameplay.')
        if args.routine == FRONTEND_SPRITES_PROBE:
            report['validation_limit'] = ('Original sprite allocation 428460/4282B0 and update 427F70 '
                                          'run unchanged with synthetic scene timing. Compares modes, forward/reverse '
                                          'ranges, speeds, pause domains and 50/60 Hz clock gates. Also compares '
                                          '465310/4653C0 setters and 465460/465590 motion controls byte-for-byte, '
                                          'with only the elapsed-time leaf replaced by explicit fixture values. Does not validate '
                                          'GPU rendering, profile widgets or playable gameplay.')
        report['runner_returncode'] = result.returncode
        if args.routine == TITLE_MENU_CONTROL_PROBE:
            report['validation_limit'] = ('Original 4D7300, its protected branch and 4D6090 close execute unchanged. Root/manager/input are bounded fixtures; '
                                          'list drawing/navigation/clear and window suspend are captured service leaves. Compares whole owner state, cooldown and ordered calls, '
                                          'not complete rendered title UI or production menu-to-race integration.')
        if args.routine == TITLE_OPTIONS_PROBE:
            report['validation_limit'] = ('Original 4D86F0 (protected row bridge unchanged), 4D8890, 4D89B0 with 4D76F0 / 4D7AB0 and the option / slider '
                                          'leaves execute; 445FE0 / 446100 / 446340 / 446430 run directly. List, input, slider tick/refresh, 4249F0, 42EFA0 '
                                          'and 42FC90 are captured leaves. Compares whole owner, license 7C23E0 and ordered calls; inits/displays are not covered.')
        if args.routine == FRONTEND_RECORD_MANAGER_PROBE:
            report['validation_limit'] = ('Original 435F70 constructor, 435FF0 cleanup including protected bridge, 48F900 and 4940D0 execute. '
                                          'SEH TLS is privately remapped; only 525F80 and deleting-virtual release leaves are captured. '
                                          'Compares whole owned records/request bytes and ordered releases over all seven-handle presence masks. '
                                          'Does not validate network requests, record fetching, complete menus or gameplay.')
        if args.routine == FRONTEND_VEHICLE_LOADER_PROBE:
            report['validation_limit'] = ('Original 48BF20/48BFE0/48BF80/48BFC0/46BBE0 execute unchanged, plus 4C8F90/4C8FC0/4474E0 and protected unlock bridges. '
                                          'Resource request/ready/release leaves are trace fixtures, not disk/GPU services. '
                                          'Compares whole objects, exact call order, all slot states, mixed readiness, flush and retained scrolling sequences. '
                                          'Unlocks cover every vehicle/colour with single-bit, empty, full and mixed license bitmaps. '
                                          'Does not validate preview model creation, original vehicle menu controller or rendered gameplay.')
        if args.routine == FRONTEND_VEHICLE_PREVIEW_PROBE:
            report['validation_limit'] = ('Original preview 48BF00/48C170/48C220/48C260/48C290/48C3F0/48C450, protected init bridge, three-slot loader and creation queue 49FA60/49FA80/4406F0 execute. '
                                          'Compares whole preview, shared loader/car/list/creation state and ordered calls over retained selection/colour/readiness/suspend/flush cycles from all30 entries, without resynchronization; all256 colour bytes/four hidden values and sixteen raw appends. Original offline 440380 is also compared for all30 models,256 colour bytes and eight player slots. '
                                          'Resource readiness, event and scene services are bounded fixtures, not real GPU or event13/event44 callbacks. Does not prove live vehicle rendering or menu-to-race integration.')
        if args.routine == VEHICLE_CONSTRUCTOR_PROBE:
            report['validation_limit'] = ('Original 5051D0,4874F0,487570,455A90,4A5830 and its protected queue bridge execute with original wheel/model/parameter tables. '
                                          'No constructor leaf is replaced. Compares whole objects, queue and shared reset with canaries, all30 models, selector flags/variants/scenes and retained sixteen-entry queues. '
                                          'Does not validate preview/game-specific initialization, renderer or full menu-to-race integration.')
        if args.routine == ENVIRONMENT_BLEND_PROBE:
            report['validation_limit'] = ('Original 44AB10 and 44A1D0 execute with their original matrix, angle, lighting and fog leaves and pinned D3DX29. '
                                          'Compares complete retained sun/fog caches, live lights, nearest indices, matrices, source/camera preservation and canaries. '
                                          'Also executes the complete 44A8DF frame sequence 4517D0/449F50/44A000, including protected 799D18 and 44C640 bridges, phase-3 44B020 and 44A520/43E570 over synthetic primary courses, over 48 retained 150-frame runs. '
                                          'Camera, vehicle, environment records and global state are synthetic. Does not validate the 44A890 mode gates, renderer binding or complete preview/game integration.')
        if args.routine == RENDER_QUEUE_PROBE:
            report['validation_limit'] = ('Original 405360/4050F0/404D10 (protected entry bridge live)/404B00/404A80/40A580 run over the real retail system sections of all 30 player car PMTs, relocated like 42E4F0, with real-perspective frustums and random views/LOD thresholds. '
                                          'Compares both queues (counts, entries with symbolic pointers, pointer arrays, matrix pools), LOD threshold, node stack depth and the matrix stack. Descriptor +20/+30 are uninitialised stack on PC and excluded. '
                                          'Does not validate the 405890 flush, materials, textures or GPU output.')
        if args.routine == VEHICLE_MODEL_DRAW_PROBE:
            report['validation_limit'] = ('Original 46AE70 executes with its two protected VM bridges live; only the render leaves 405360/4056D0/4044F0/404540/4052B0/4052C0/405350 are capture stubs recording arguments and the current matrix. '
                                          'Compares the complete ordered draw list, the car object (flags, +2FC, +B0 normalisation), matrix arena and stack over 3000 randomised runs covering all 30 models, all view modes, alt paths, debug override, roof, steering and the 449640 path. '
                                          'Does not validate object/material realisation, GPU output or the original shadow draw (49A650 is RET in this build).')
        if args.routine == CAR_SERVICES_GHOST_PROBE:
            report['validation_limit'] = ('Original 47ED90, 47F330, 466E50 and the codecs 466A20/466AF0/466030/4667D0/4668F0/466720 execute whole and unpatched with pinned D3DX29 '
                                          '(inverse, transform coord, rotations) and the protected 466AF0 bridge live; the bridge is also measured as a black box (all registers at 466AFA, .data unchanged). '
                                          'Compares recorder/record slots, car, packet buffer, 7F8EF4/7F8EF8/7F91F0/7F92C8/7F9300 and matrix arena/stack. Inputs are synthetic. '
                                          'Does not validate the 47F780/4671D0 callers or ghost playback.')
        if args.routine == CAR_SERVICES_PROBE:
            report['validation_limit'] = ('Original 43D130, 4963E0, 456BB0, 4F53B0, 450130, 4B5FD0, 44F0F0, 45A2B0, 46F7A0, 479D90, 46FAC0/46FC40/46FDE0 (+44C680) execute whole with the protected '
                                          'VM bridges and relocated snippets live; 4A3F80 and 4A45F0 run with only their protected selectors restored (43E3B0 and 43D1D0 stay r046/r041 stubs). '
                                          'The async file routines 44FD20/44F880/44FBE0 are capture stubs (platform I/O boundary). Inputs are synthetic. 479D90 ST0 is exact for x87 0x037f only.')
        if args.routine == CAMERA_PROBE:
            report['validation_limit'] = ('Pinned d3dx9_29 generic D3DXMatrixInverse/PerspectiveOffCenterRH executed directly (its msvcrt _finite import is an exact stub). '
                                          'Original 484BD0 and 484EE0 execute with 483C10, 404310, D3DXPlaneFromPoints, 482F20, 4493E0/4B5FD0 bridge, 449580, 40F180, rotations and 40A240; only the Direct3D-facing 409DF0/409E00 are capture stubs. '
                                          'Camera objects, screen sizes and blend inputs are synthetic. Also 485FE0 head, 482E80, 4A2BA0 over game modes 12..31 and the mode-32 frontend preset branch. Does not validate the other 485FE0 controllers (+94/+98/+9C, race modes), shader-constant upload, GPU output or menu integration.')
        if args.routine == VEHICLE_PREVIEW_INIT_PROBE:
            report['validation_limit'] = ('Original 4A7270 executes unpatched with 4A5830, 449F50 (protected 799D18 bridge, 44B020/44AB10, 44A1D0), 4A69F0, 519300, 4F6E40, 49A650 and 4A2EE0; only the runtime-selected 45A2B0 rank provider is a fixture stub. '
                                          'Compares whole event/canary, 82E7F0 body, queue, 83DB30, environment globals/records/lights and world/matrix state for 30 models x 12 scene/transmission/phase/variant setups. '
                                          'Course, camera and environment records are synthetic. Does not validate event181/display, all-car GPU, transmission owner or menu-to-race integration.')
        if args.routine == VEHICLE_BODY_PROBE:
            report['validation_limit'] = ('Original 4A69F0, protected clear, 516E10,4A6840,4A1E80,4A1FC0,519300,4F6E40 and original world/grid queries execute with pinned D3DX29. '
                                          'Compares whole event/body/canaries and world/matrix/prediction state for 19 parameter columns, 16 speed/angle/course cases and 256 three-probe ground cases; all30 model collision descriptors over16 memory patterns. '
                                          'Full original4A5B20/4A2650/4A2910/4A4710 run retained128-frame sequences for30 models/eight global configurations, including original model lookup and CRT rand; only its Windows thread-data lookup is adapted. Legacy reverse-car leaf patches are explicitly removed. '
                                          'Course geometry and global inputs are synthetic. Does not validate complete preview/game initialization, renderer, live course integration or menu-to-race gameplay.')
        if args.routine == FRONTEND_VEHICLE_MENU_PROBE:
            report['validation_limit'] = ('Original 4C9010/4C9290/4C8D70/4C8D90 and the complete 51B7B0 carousel execute, including protected unlock and left-navigation bridges. '
                                          'Preview, committed-selection, transition, input, GPU, labels and sound services are bounded fixtures/captures. '
                                          'Compares carousel state, retained colours, next key/action and resource arguments for all30 selections, eight colours, three profiles and combined buttons; display/suspend with either dialog state and dialog confirm/cancel/idle. '
                                          'Does not validate constructor/destructor, real preview rendering, transmission owner or the complete menu-to-race route.')
        if args.routine == FRONTEND_FIXED_CHOICE_PROBE:
            report['validation_limit'] = ('Original key1/key2 init/control and nested 51B7B0..51BF06 widget code execute, including protected bridges and original tables. '
                                          'Input, graphics allocation/configuration, audio, command labels and external managers are fixtures/captures. '
                                          'Compares initialized child, controller/widget tail, mode fields, action latches and animation arguments. '
                                          'Does not validate the external managers, hardware pixels or the entire menu-to-race path.')
        if args.routine == FRONTEND_INPUT_PROBE:
            report['validation_limit'] = ('Original 48F4F0/48F5F0 including the protected comparison execute unchanged. '
                                          'Device/axis/feature reads are explicit snapshot fixtures; 440ED0 feedback is captured. '
                                          'Compares priorities, shared previous action, full objects, repeat traces and feedback arguments. '
                                          'Does not validate Switch hardware mapping or audible feedback.')
        if args.routine == FRONTEND_COMMANDS_PROBE:
            report['validation_limit'] = ('Original 446C50/446D90/446EA0/446FC0/447000/447090 execute with the original protected bridge and table. '
                                          'Whole history/resource objects and ordered configure/finalize calls are compared. '
                                          'Graphics configuration, commit, release and status are explicit capture/fixture leaves. '
                                          'Does not validate rendered button labels, production menu input or complete menus.')
        if args.routine == FRONTEND_LIST_PROBE:
            report['validation_limit'] = ('Original list constructors, text/sprite append, navigation, visibility, scrolling text and display composition. '
                                          'Windows allocator/SEH use a private fixture arena; GPU submissions and audio calls are captured, '
                                          'animation allocation leaves are stubbed; FONT residency uses a COM texture-description fixture. '
                                          'Original font measurement and glyph placement execute. Window move/rectangle and retained 48CC00 motion '
                                          'run with the original vector arithmetic and explicit owner-delta fixtures. '
                                          'Window 48D0C0 init, 48CEF0 text height, 48C5F0 glyph/border/button composition and '
                                          '4E12B0/4E1680 context open/close execute original nested window/list bodies. '
                                          'CRT formatting supports fixture literals and original %s only; localization uses retail strings. '
                                          'Delete 4E16A0 open and 4E17E0 control execute original nested 48D970 full-widget choice lists, '
                                          'row construction, selection, glyph placement, animation arguments and cleanup. Owner input is a virtual-call fixture. '
                                          'Chooser 4E1890 nested construction, 4E19E0 initialization (including the original protected eighth argument), '
                                          '4E1A70 reset and 4E1AC0 suspend compare whole objects, common save state, active/bank profiles and restored indices. '
                                          'Title 4D5E40 executes nested sprite rows 48E390 and window initialization across root/variant/manager states; '
                                          '48DDA0 sprite/no-highlight display compares ordered animation configuration arguments. '
                                          'Owner 442F20/440EA0 commands and profile audio/control leaves are captured or stubbed, not production implementations. '
                                          'This does not validate image-bank GPU rendering, persistent profile deletion, production menu ownership or complete menus.')
        if args.routine == FRONTEND_LICENSE_PANELS_PROBE:
            report['validation_limit'] = ('Original 4E2150 panel population, 4E2640 placement, 4E1CF0 profile actions and 449B30 time conversion. '
                                          'Special 4E0DD0/4E1060 manager-provided cards execute original pointer lookup, formatting and placement; '
                                          '4EEEE0 rank thresholds are compared at equality and adjacent floats. Manager records are fixtures, not live networking. '
                                          'Text and sprite creation leaves capture original arguments; completion percentage and localization are fixtures. '
                                          '447400 completion chain is also independently executed, with category counts and championship tables as fixtures. '
                                          'Shared category grades, 4E8410 unlocks and 4E8B20 picture selection execute original code. '
                                          'Key3A 4E8780 control and 4E8C60 display are compared for action, next key, mode bits, selection and nine resource configurations; input/audio/GPU/font leaves are fixtures. '
                                          'Key3B 4E92D0 initialization, 4E9360 control and 4E95A0 display execute original code and protected bridges across all 40 nodes/eight grades/map and race modes. 4958C0 resolves retail Races/assignment records with archive accessor fixtures. Compare selected bits, next child, node/cursor, sprite descriptors and localized caption IDs; native resource teardown checked. Original input/audio/GPU/text-output remain leaves, not a full menu or race execution. '
                                          'Context dialog, save/delete and audio leaves are fixtures, not actual dialogs or persistence validation. '
                                          'Checks sparse bank synchronization, text, formatting, sprites, positions, context flags and transitions, not GPU pixels or complete menus.')
        if args.routine == FRONTEND_GLYPH_RENDER_PROBE:
            report['validation_limit'] = ('Original 42A0A0 glyph drawing arithmetic runs unchanged. '
                                          'Texture dimensions are fixtures; COM draw and matrix leaves capture crop, color, scale and translation. '
                                          'Compares native quad geometry and UVs, not GPU pixels, sprite-bank orientation or complete menus.')
        if args.routine == FRONTEND_IMAGE_RENDER_PROBE:
            report['validation_limit'] = ('Original 42C2F0, 42D280, 42D300, 42A3A0 and 42A0A0 image arithmetic runs against controlled bank-3 crop records. '
                                          'Includes final 42A3A0 UV permutation, 0.51-texel offsets and 0.5-pixel position offsets. '
                                          'COM methods, identity transform and render-state leaves are fixtures; final vertex bytes are captured. '
                                          'No GPU pixels, non-identity custom material matrices or complete menu validation.')
        if args.routine == FRONTEND_KEYBOARD_PROBE:
            report['validation_limit'] = ('Original keyboard tick, navigation, alphabets, name limits and commit/cancel decisions. '
                                          'Input, sound capture and graphics leaves are fixtures; graphics readiness is explicit. '
                                          'Name/controller bytes and sound decisions are compared, not GPU pixels or resource bytes. '
                                          'Native editor integration separately uses real ETC animations. No complete menu claim.')
        if args.routine == FRONTEND_TEXT_PROBE:
            report['validation_limit'] = ('Original text glyph placement/clipping, kerning, measurement, '
                                          'word wrapping, widget setters and three-panel construction/init run against retail font metrics. '
                                          'The GPU submission leaf is replaced by a byte capture; panel Windows SEH TLS accesses '
                                          'are remapped to private storage. No panel child constructor is stubbed. '
                                          'This does not validate pixels on Switch or complete menu navigation.')
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        print(f"Wrote {output}; mismatching cases: {report.get('total_mismatching_cases', 'unknown')}")
        return 0 if result.returncode == 0 and report.get('total_mismatching_cases') == 0 else 1
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 2

if __name__ == '__main__':
    raise SystemExit(main())
