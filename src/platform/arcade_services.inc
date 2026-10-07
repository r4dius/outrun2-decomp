        // ---- OUTRUN2SP arcade frontend (arcade_attract.cpp) and modes 7 / 9 / 10 / 11 / 12 ----
        // Offline network session words: CommRace block 7DE418.. (7DF10A count, 7DF10B index,
        // 7DF110 positions, 7DF1A0, 7DF1AC, 7DF1B8); 7DF34C (the network manager) stays 0.
        case 0x455ab0u:if(st.network_manager_7df34c_present)throw EndUnported(k.pc,"network manager present");return 0u;   // xor ax,ax
        case 0x455b40u:case 0x45aa50u:case 0x45a920u:
            if(st.network_manager_7df34c_present)throw EndUnported(k.pc,"network manager present");return 1u;
        case 0x45a840u:if(st.network_manager_7df34c_present)throw EndUnported(k.pc,"network manager present");return 0u;   // eax = [7DF34C] = 0: returns
        case 0x455b00u:return commrace(0x7df110u+a[0]);
        case 0x455b10u:return commrace(0x7df10au);
        case 0x455c10u:return std::uint32_t(std::int32_t(std::int8_t(commrace(0x7df10bu)))+1);
        case 0x455c20u:return std::uint32_t(std::int32_t(std::int8_t(commrace(0x7df10au))));
        case 0x455660u:for(std::uint32_t i=0;i<4;++i)commrace_put(0x7df1a0u+i,std::uint8_t(a[0]>>(i*8)));return 0u;
        case 0x4556e0u:commrace_put(0x7df1b8u,std::uint8_t(a[0]));return 0u;
        case 0x48f970u:return 0u;                                  // xor al,al
        case 0x47f110u:return 0u;                                  // the current player selector (closed: 0)
        case 0x49b460u:return m.u32(0x8369c0u);
        // LAN session services (the 4C05F0 online branch). The session 7D68AC lives in the
        // network module's memory (0 when it never ran: the offline results).
        case 0x455ad0u:return c.race.car_world.slot_7dd138;          // mov al,[7DD138]
        case 0x455ae0u:{                                             // player a[0]'s session byte +9C
            const std::uint32_t s=native_network_u32(0x7d68acu);
            if(!s||std::int32_t(a[0])>=std::int32_t(native_network_u32(s+0x14u)))return 0u;
            return native_network_u8(native_network_u32(s+0x18u+a[0]*4u)+0x9cu);}
        case 0x4591f0u:return native_network_u32(0x7d68acu)?commrace(0x7de425u+a[0]*0x6cu):0u;
        case 0x4591c0u:{
            if(!native_network_u32(0x7d68acu))return 0u;
            const std::uint32_t v=commrace(0x7de426u+a[0]*0x6cu);return v<1u?1u:v>8u?8u:v;}
        case 0x457a60u:{   // the race slots: the local player's CommRace index, the others in order
            const std::uint32_t count=commrace(0x7de418u),total=commrace(0x7de418u)|commrace(0x7de419u)<<8|commrace(0x7de41au)<<16|commrace(0x7de41bu)<<24;
            c.start_mode.network_player_count_7df10f=std::uint8_t(count);c.start_mode.network_mode_7df108=1u;
            const std::uint32_t s=native_network_u32(0x7d68acu),p=s?native_network_u32(s+0x58u):0u;
            const std::uint32_t id=p?(native_network_u8(p)|native_network_u8(p+1u)<<8):0xffffffffu;
            std::uint32_t self=0xffffffffu;
            for(std::uint32_t i=0;i<total;++i){
                const std::uint32_t at=0x7de420u+i*0x6cu;
                if((commrace(at)|commrace(at+1u)<<8|commrace(at+2u)<<16|commrace(at+3u)<<24)==id){self=i;break;}
            }
            auto car_slot=[&](std::uint32_t cell,std::uint32_t v){   // [cell]+1054 = v, +10 = (byte)v
                if(!m.mapped(cell,4))return;const std::uint32_t car=m.u32(cell);
                if(car&&m.mapped(car+0x1054u,4)){m.put32(car+0x1054u,v);m.put8(car+0x10u,std::uint8_t(v));}};
            car_slot(0x799d18u,self);
            commrace_put(0x7df110u,std::uint8_t(self));commrace_put(0x7df10bu,std::uint8_t(self));
            c.race.car_world.slot_7dd138=std::uint8_t(self);
            for(std::uint32_t i=1,v=0;std::int32_t(i)<std::int32_t(count);++i,++v){
                if(v==self)++v;
                car_slot(0x799d54u+(i-1u)*0x3cu,v);commrace_put(0x7df110u+i,std::uint8_t(v));
            }
            for(std::uint32_t i=0;std::int32_t(i)<std::int32_t(count);++i)
                for(std::uint32_t b=0;b<4;++b)commrace_put(0x7de478u+i*0x6cu+b,std::uint8_t(i>>(b*8)));
            return 0u;}
        case 0x495a10u:   // the network race settings record ([83637C] = 84A9B0)
            mission.network_record_83637c=a[0];native_network_put32(0x83637cu,a[0]);return 0u;
        // 455B60 looks up the slot's 957A6C object (42E030) and returns 0 (xor eax,eax). 455BF0 returns
        // that object's +74 byte; its only caller 4BF570 draws a mark only where 455B60 is nonzero,
        // so neither value can change a frame: both answer 0.
        case 0x455b60u:case 0x455bf0u:return 0u;   // see above: no frame can change
        // input (47F110 = 0): 7D6770 switch words, 7D6810 analogue channels
        case 0x4536c0u:return c.race.car_world.switch_7d6770.held&a[0];
        case 0x453720u:if(a[0]>=7u)throw EndUnported(k.pc,"analogue channel");
            return std::uint32_t(c.race.car_world.analog_7d6810.current[a[0]]);
        case 0x453750u:if(a[0]>=7u)throw EndUnported(k.pc,"analogue channel");return std::uint32_t(c.race.car_world.analog_7d6810.previous[a[0]]);
        case 0x453780u:
            if(a[0]==1u)return std::uint32_t(c.race.car_world.analog_7d6810.accel_edge_1c);
            if(a[0]==2u)return std::uint32_t(c.race.car_world.analog_7d6810.brake_edge_2c);
            throw EndUnported(k.pc,"analogue channel edge");
        // frontend globals
        case 0x43f950u:st.course_preset=a[0];return 0u;          // [78024C]
        case 0x40ec60u:c.event_function36.car_select.clear_colour_89bd5c=a[0];return 0u;
        case 0x417f70u:return 0u;                                  // [8A8CE0]: only 417740 writes it (0)
        case 0x44c0a0u:c.event_function36.car_select.course_mode_7d31d0=std::uint8_t(a[0]);return 0u;
        case 0x48b130u:c.start_mode.course_choice_655b59=a[0]&0xffu;return 0u;
        case 0x48b150u:c.start_mode.vehicle_colour_655b5a=std::uint8_t(a[0]);return 0u;
        case 0x48b170u:c.event_function36.car_select.transmission_830374=std::uint8_t(a[0]);return 0u;
        case 0x48b190u:st.vehicle_variant_83036d=std::uint8_t(a[0]);return 0u;
        case 0x48b1b0u:c.race.sound.option_83036c=std::uint8_t(a[0]);return 0u;
        case 0x48b1c0u:return c.race.sound.option_83036c;
        case 0x48b1d0u:c.event_function36.music_globals.track_830364=std::uint8_t(a[0]);return 0u;
        case 0x48b200u:e.word_830370=std::uint32_t(std::int32_t(std::int8_t(a[0])));return 0u;
        case 0x47ef20u:e.word_813744=a[0];return 0u;
        case 0x4957d0u:st.selection_active_836374=true;return 0u;
        case 0x4518c0u:st.first_race_flags_7d3a10=0u;return 0u;   // 7D3A10 / 7D3A2C / 7D3A48 (START rebuilds them)
        case 0x580f40u:return frontend_crt_random_580f40(c.event_function36.pc_crt_random_state);
        case 0x46bbe0u:{std::uint32_t v{};if(!exe_u32(0x5b2fe0u+a[0]*8u,v))throw EndUnported(k.pc,"vehicle resource table");return v;}
        case 0x449fa0u:return 0x7d26a8u+a[0]*4u;                 // the sun / texture lists (car select preview)
        case 0x448fd0u:{
            for(std::uint32_t b=0;b<9;++b)if(a[0]&(1u<<b))e.callbacks_7d2620[b]=a[1];
            return 0u;}
        // resources 7C2800 (the car select preview loader)
        case 0x448ad0u:
            // the arcade ending (mode 24, 4527F0): its course-side models go to the AREA owner's
            // resource model (7C2800 entries, paths 633558, banks loaded when drawn)
            if(c.mode_state.current==24u&&c.race_area)return native_race_area_call(c,nullptr,k);
            if(a[0]==0xbau||a[0]==0xbbu){   // the bootstrap loader entries: already resident, ready again at once
                auto& entry=c.event_function36.loader_resource_entries[a[0]==0xbau?0:1];std::uint32_t pending=0;
                if(driving::runtime_resource_request_448ad0(entry,a[0],a[1],pending))entry.status_14=7u;
                return 0u;}
            if(!native_frontend_resource_request(c,a[0],a[1]))throw EndUnported(k.pc,"resource request "+hex(a[0])+" mode "+hex(a[1]));return 0u;
        case 0x448960u:{bool ready=false;if(!native_frontend_resource_ready(c,a[0],ready))throw EndUnported(k.pc,"resource state");return ready?1u:0u;}
        case 0x448980u:
            if(c.mode_state.current==24u&&c.race_area)return native_race_area_call(c,nullptr,k);
            return c.event_function36.car_select.pending_7cc1d8==0u?1u:0u;
        case 0x448b90u:return native_object_db_step_448b90(c);
        case 0x44da00u:native_runtime_load_course(c,a[0],std::uint8_t(a[1]));return 1u;
        // course data copies of the AREA block
        // (the course runtime of 44DA00 owns 7D2DE0 / 7D30A8: copied there, not through the AREA block)
        case 0x44c0b0u:{auto& rt=c.game_mode.course_runtime;rt.selected_7d30a8=rt.selected_7d2de0;return 0u;}
        case 0x44c0d0u:{   // the course matrices of the selected record's primary descriptor (+1C)
            auto& g=c.game_mode;auto& rt=g.course_runtime;
            std::uint32_t d;std::memcpy(&d,rt.selected_7d30a8.data()+0x14u,4);
            if(!d)return 0u;                                      // null fast path
            if(!g.course_assets)throw EndUnported(k.pc,"course descriptors not loaded");
            const auto tables=driving::course_descriptor_tables_r078(g.course_assets->descriptors);
            std::uint32_t idx;std::memcpy(&idx,rt.selected_7d30a8.data()+0x1cu,4);
            if(!tables.primary||idx>=tables.primary_count)throw EndUnported(k.pc,"course descriptor index");
            driving::Bytes mb(g.course_matrix_stack.data(),g.course_matrix_stack.size());
            for(std::size_t i=0;i<16u;++i)mb.put32(i*4u,(i%5u)==0u?0x3f800000u:0u);
            driving::PcMatrixStack stack{mb,0,0,2};
            driving::runtime_course_matrix_44c0d0(tables.primary[idx],stack,rt);
            return 0u;}
        case 0x44fcc0u:case 0x44fce0u:{   // 44FB60: no area loader slot 7D34D0.. busy -> 44FCC0 [7D34C4] = arg, [7D34A8] = 0 / 44FCE0 [7D34C0] = arg
            if(c.race_area){
                bool idle=true;
                for(const auto& q:c.race_area->slots)
                    if(q.pc.word0!=0u&&std::int32_t(q.pc.state)>=0&&std::int32_t(q.pc.state)<=2){idle=false;break;}
                if(idle){
                    if(!m.mapped(0x7d34c0u,8)||!m.mapped(0x7d34a8u,4))throw EndUnported(k.pc,"area loader words 7D34A8 / 7D34C0 / 7D34C4");
                    if(k.pc==0x44fcc0u){m.put32(0x7d34c4u,a[0]);m.put32(0x7d34a8u,0);}else m.put32(0x7d34c0u,a[0]);}
                return 0u;}
            // no AREA owner (dropped by 44FD00): its .bss is zero, the next owner starts empty
            if(a[0]!=0u)throw EndUnported(k.pc,"44FCC0 / 44FCE0 with no AREA owner");
            return 0u;}
        case 0x4f21b0u:{   // [84DE64] != 1: the RobMotion scheduler is not loading
            const auto& block=c.race.robots.motion_tables.state.block;std::uint32_t v;
            std::memcpy(&v,block.data()+(0x84de64u-PcRobMotionTablesState::Base),4);return v!=1u?1u:0u;}
        // bookkeeping (rankings book [63960C], stats 7D6720..)
        case 0x452ed0u:sp_stats_set_452ed0(m,a[0],a[1]);return 0u;
        case 0x452cd0u:sp_stats_count_452cd0(m,a[0],a[1]);return 0u;
        case 0x452d20u:sp_stats_average_452d20(m,a[0],a[1]);return 0u;
        case 0x453320u:return m.u32(0x7d673cu);
        // the vehicle creation queue 841AC0 (49FA60 reset: protected head 841BC9 = 0)
        case 0x49fa60u:if(!vehicle_creation_reset_49fa60(c.vehicle_creation))throw EndUnported(0x49fa60u,"vehicle creation queue");return 0u;
        // sprites: 4288C0 the frame of a pool sprite
        case 0x4288c0u:if(!pool.set_frame(a[0],std::int32_t(a[1])))++e.released[0x4288c0u];return 0u;
        case 0x42d5c0u:{
            if(!e.hud_draws)throw EndUnported(0x42d5c0u,"2D image outside a display");
            RaceHudDraw d;d.pc=k.pc;for(std::uint32_t i=0;i<5;++i)d.args[i]=a[i];e.hud_draws->push_back(d);return 0u;}
        case 0x42c360u:e.text.style.flags=a[0];return 0u;
        case 0x408880u:++e.released[0x408880u];return 0u;          // D3D render states for the 2D panel (no queue effect)
        // the 446xxx embedded slots object 84A320 (FrontendUiResources over the frontend pool)
        case 0x446d90u:case 0x446cf0u:case 0x447000u:case 0x447090u:case 0x4470f0u:{
            if(k.ecx!=0x84a320u)throw EndUnported(k.pc,"embedded slots outside 84A320");
            FrontendUiResources ui{pool};ui.motion_step=c.event_function36.frontend_ui_motion_step;
            ui.effect_user=c.event_function36.frontend_effect_user;ui.effect_4249f0=c.event_function36.frontend_effect;
            driving::Bytes object(e.state.arcade_84a208.data()+(0x84a320u-0x84a208u),0x68cu);
            std::uint32_t result{};
            if(!ui.commands(k.pc,object,a,k.argc,c.event_function36.frontend_ui_globals,result))throw EndUnported(ui.missing_pc?ui.missing_pc:k.pc,"embedded slots");
            return result;}
        case 0x446a50u:{                                          // 446A50: the four slot captions (446510)
            if(k.ecx!=0x84a320u)throw EndUnported(k.pc,"embedded slots outside 84A320");
            const driving::Bytes object(e.state.arcade_84a208.data()+(0x84a320u-0x84a208u),0x68cu);
            if(!object.u8(0))return 0u;
            const auto* fonts=c.event_function36.frontend_fonts;const auto* table=c.event_function36.frontend_text;
            for(std::uint32_t i=0;i<4u;++i){
                const std::uint32_t row=(i+object.u32(0x408u)*8u)*4u;
                if(object.u32(row+8u)==0xffffffffu||object.u32(row+0x18u)==0xffffffffu)continue;
                if(!fonts||fonts->fonts[9].token!=9u)throw EndUnported(0x42ca60u,"slot captions: font 9 not loaded");
                if(!e.hud_glyphs)throw EndUnported(0x446a50u,"slot captions outside a display");
                e.text.font=&fonts->fonts[9];e.text.style.scale_x=1.0f;e.text.style.scale_y=1.0f;
                e.text.style.mode=0xeu;e.text.style.color=0xffffffffu;
                float fx{},fy{};std::uint32_t off{};
                if(!exe_f32(0x59dc84u+i*8u,fx)||!exe_f32(0x59dc88u+i*8u,fy)||!exe_u32(0x59dca4u+i*4u,off))throw EndUnported(0x446510u,"caption positions");
                std::int32_t x=cvtt_end(fx);
                if(i>1u){e.text.style.flags=0xau;x-=0x14;}else{e.text.style.flags=9u;x+=0x11;}
                x=x-std::int32_t(off)+0x140;
                e.text.cursor.x=std::int16_t(x);e.text.cursor.y=std::int16_t(cvtt_end(fy)+0xf0);
                e.text.cursor.origin_x=e.text.cursor.x;e.text.base_y=e.text.cursor.y;
                const std::string* s=table?table->get(object.u32(row+0x18u)):nullptr;
                if(!s)throw EndUnported(0x465eb0u,"slot caption text");
                if(!frontend_text_draw(*e.text.font,e.text.style,e.text.cursor,*s,*e.hud_glyphs))throw EndUnported(0x42cdd0u,"slot caption outside the font");
                e.text.style.flags=1u;
            }
            return 0u;}
        // the course select picture (\common\sel_dl_edit0.tgt) for the 7D26A8 texture slots
        case 0x4239c0u:{
            std::string name;for(std::uint32_t p=a[0];name.size()<0x100u;++p){const auto ch=m.u8(p);if(!ch)break;name+=char(ch);}
            auto* store=c.event_function36.retail_assets;
            std::string rel=name;for(auto& ch:rel)if(ch=='\\')ch='/';if(!rel.empty()&&rel[0]=='/')rel.erase(0,1);
            e.picture_file.clear();
            if(!store||!retail_asset_read_relative(*store,rel,e.picture_file,0x100000u)||e.picture_file.empty())return 0u;
            return 1u;}
        case 0x423cb0u:if(a[3]!=1u)throw EndUnported(k.pc,"picture handle");e.picture_target=a[0];e.picture_w=a[1];e.picture_h=a[2];++e.pictures_loaded;return 0u;
        case 0x423bd0u:return 0u;   // 423BD0 picture file close: the bytes stay in e.picture_file (no PC handle)
        // events 4 / 8: 4406A0(id, n) = event 8 function 0x2B + the 49FA80 record (id, n clamped 1..8, 4081)
        case 0x4406a0u:{
            driving::event_setup_440110(c.event_state,8u,0x2bu,c.event_descriptors,c.event_functions);
            std::int8_t n=std::int8_t(a[1]);if(n<1)n=1;else if(n>8)n=8;
            if(!vehicle_creation_append_49fa80(c.vehicle_creation,8u,0u,a[0],std::uint32_t(std::uint8_t(n)),0x4081u))throw EndUnported(0x49fa80u,"49FA80 record");
            return 0u;}
        case 0x440200u:{
            auto& slot=c.event_state.slots[a[0]];
            if(slot.flags&1u){slot.flags&=0xfeu;return 0u;}
            if(!(slot.flags&2u))return 0u;
            slot.flags=0;
            if(!slot.dest_callback||slot.dest_callback==0x49a650u)return 0u;
            if(a[0]==4u&&arcade_event4_invoke(ctx,slot.dest_callback,0x780440u))return 0u;   // jmp ecx: dest(work)
            throw EndUnported(0x440200u,"event destroy callback "+hex(slot.dest_callback));}
        // sound
        case 0x427b10u:race_sound_preview_427b10(c.race.sound.state,std::int8_t(c.race.sound.option_83036c),a[2]!=0u);return 0;   // arg3 = held
        // the arcade entry module (4B6EE0 entry select, 4B88D0..4B8C00 network wait banner)
        case 0x4b6ee0u:case 0x4b88d0u:case 0x4b89b0u:case 0x4b89e0u:case 0x4b8c00u:{
            std::uint32_t r=0;(void)arcade_entry_invoke(ctx,k,r);return r;}
        // START scene owner (mode 10 attract: 49BA50 reset; 49BA80 only when [830380] is set, never on PC)
        case 0x49ba50u:native_scene_owner_reset_49ba50(c);return 0u;
        case 0x49ba80u:throw EndUnported(k.pc,"attract demo race (830380) not reachable");
