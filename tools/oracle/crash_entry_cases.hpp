// r021 candidate: direct original entries, including protected dispatch. The
// accepted corpus does NOT request active PlWrecker; it is never replaced.
bool entry_enabled(const std::string& only){if(only=="all"||only=="crash_entry_all")return true;for(auto n:outrun::testing::entry_names)if(only==n)return true;return false;}
constexpr std::uint32_t entry_pages[]={0x5e0000,0x5e1000,0x5e3000,0x5c2000,0x64d000,0x80f000,0x7d2000,0x7d3000,0x780000,0x89b000,0x956000,0x79f000,0x85f000};
constexpr unsigned entry_page_count=sizeof(entry_pages)/sizeof(entry_pages[0]);
void init_entry_oracle(const std::string& only){if(!crash_enabled(only))map_at(CrashArena,0x10000,PROT_READ|PROT_WRITE);for(auto p:entry_pages)if(mprotect(reinterpret_cast<void*>(p),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("entry globals mprotect");}
struct EntryBinding {
 std::array<std::array<std::uint8_t,4096>,entry_page_count> saved{},expected{};
 explicit EntryBinding(outrun::testing::CrashEntryFixture& f){
  for(unsigned k=0;k<entry_page_count;++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(entry_pages[k]),4096);
  // Reuse the previous binding, but retain our own independent full snapshots.
  {CrashBinding cb(f);cb.capture();for(unsigned k=0;k<entry_page_count;++k)std::memcpy(expected[k].data(),reinterpret_cast<void*>(entry_pages[k]),4096);}
  for(unsigned k=0;k<entry_page_count;++k)std::memcpy(reinterpret_cast<void*>(entry_pages[k]),expected[k].data(),4096);
  auto b=f.bytes();auto put=[](std::uint32_t p,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(p)=v;};
  put(0x7d33bc,CrashArena+0x5400);put(0x7d33c4,b.u32(0x59a4));put(0x7d2df4,CrashArena+0x5640);
  put(0x64deec,CrashArena+0x5900);put(0x64def0,CrashArena+0x5920);put(0x80fb14,b.u32(0x59a0));
  const std::uint32_t bounds[][3]={{0x5c2570,0x5700,256},{0x5e0f20,0x5800,256},{0x5e0df0,0x5a00,64}};
  for(const auto& x:bounds)std::memcpy(reinterpret_cast<void*>(x[0]),f.image.data()+x[1],x[2]);
  put(0x5e3028,b.u32(0x59c0));put(0x5e0ac0,b.u32(0x59c4));put(0x5e3024,b.u32(0x59d0));put(0x5e3020,b.u32(0x59d4));
  for(unsigned k=0;k<entry_page_count;++k)std::memcpy(expected[k].data(),reinterpret_cast<void*>(entry_pages[k]),4096);
 }
 void capture(){for(const auto& x:crash_mutable)std::memcpy(reinterpret_cast<void*>(CrashArena+x[1]),reinterpret_cast<void*>(x[0]),x[2]);}
 void compare(const std::string& name,outrun::testing::CrashEntryFixture& f){
  compare_world_bytes(name+"_memory",reinterpret_cast<void*>(CrashArena),f.image.data(),f.image.size());
  for(const auto& x:crash_mutable){unsigned k=0;while(entry_pages[k]!=(x[0]&~4095u))++k;std::memcpy(expected[k].data()+(x[0]&4095u),f.image.data()+x[1],x[2]);}
  for(unsigned k=0;k<entry_page_count;++k)compare_world_bytes(name+"_globals_"+std::to_string(k),reinterpret_cast<void*>(entry_pages[k]),expected[k].data(),4096);
 }
 ~EntryBinding(){for(unsigned k=0;k<entry_page_count;++k)std::memcpy(reinterpret_cast<void*>(entry_pages[k]),saved[k].data(),4096);}
};
std::uint32_t run_entry_original(unsigned id){
 const std::uint32_t entries[]={0x46c5b0,0x46c6e0,0x46c780,0x4a2270,0x4a6ea0,0x5038d0};
 if(id==9)return run_crash_original(10);
 prepare(entries[id]);Bytes b(reinterpret_cast<void*>(CrashArena),outrun::testing::crash_image_size),st(reinterpret_cast<void*>(S),64);
 switch(id){
 case 0:st.put32(0,b.u32(0x59d8));st.put32(4,b.u32(0x59bc));st.put32(8,(b.u32(0xc)>>25)&1);for(unsigned k=0;k<6;++k)st.put32(12+k*4,CrashArena+0x5b00+k*4);break;
 case 1:guest_call.esi=CrashArena;break;
 case 2:st.put32(0,CrashArena);st.put32(4,b.u32(0x59a8)%16);st.put32(8,b.u32(0x59dc));break;
 case 3:case 4:st.put32(0,CrashArena);st.put32(4,b.u32(0x59a8));st.put32(8,b.u32(0x59ac));st.put32(12,b.u32(0x59b0));st.put32(16,b.u32(0x59b4));break;
 case 5:guest_call.esi=CrashArena;st.put32(0,CrashArena+0x1200);st.put32(4,b.u32(0x59b8));st.put32(8,CrashArena+0x4400);break;
 }
 run();return id==5?guest_call.out_eax:0;
}
std::ofstream entry_golden;std::uint32_t entry_golden_count=0;
void entry_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));entry_golden.write(b,4);}
void begin_entry_golden(const char* path){entry_golden.open(path,std::ios::binary|std::ios::trunc);if(!entry_golden)throw std::runtime_error("entry capture open");entry_golden.write("OR2R021\0",8);entry_u32(1);entry_u32(x87_control);entry_u32(0);entry_u32(outrun::testing::crash_image_size);}
void finish_entry_golden(){if(entry_golden.is_open()){entry_golden.seekp(16);entry_u32(entry_golden_count);entry_golden.flush();if(!entry_golden)throw std::runtime_error("entry capture write");entry_golden.close();}}
std::map<std::string,std::uint64_t> entry_coverage;
void run_entry_cases(unsigned i,const std::string& only){
 if(!entry_enabled(only))return;
 WorldNativeControl control(static_cast<unsigned short>(x87_control));
 for(unsigned id=0;id<outrun::testing::entry_selector_count;++id){
  if(only!="all"&&only!="crash_entry_all"&&only!=outrun::testing::entry_names[id])continue;
  auto f=outrun::testing::make_entry_fixture(i,id);EntryBinding binding(f);
  const unsigned stages=outrun::testing::entry_stage_count(id);const bool capture=entry_golden.is_open()&&i<128;
  if(capture){entry_u32(id);entry_u32(i);entry_u32(stages);entry_golden.write(reinterpret_cast<const char*>(f.image.data()),f.image.size());++entry_golden_count;}
  for(unsigned stage=0;stage<stages;++stage){
   const auto actual=outrun::testing::entry_stage_id(id,stage);auto before=f.image;
   const auto original=run_entry_original(actual);binding.capture();
   if(capture){entry_u32(original);entry_golden.write(reinterpret_cast<const char*>(CrashArena),f.image.size());}
   const auto native=outrun::testing::run_entry_native(f,actual);
   const auto name=std::string(outrun::testing::entry_names[id])+(stages>1?"_"+std::to_string(stage+1):"");
   const auto old=stats[name+"_memory"].fail+stats[name+"_return"].fail;
   compare_u32(name+"_return",original,native);binding.compare(name,f);
   if(old!=stats[name+"_memory"].fail+stats[name+"_return"].fail)std::cerr<<"entry case="<<i<<" selector="<<name<<" flags="<<std::hex<<Bytes(before.data(),before.size()).u32(0xc)<<std::dec<<"\n";
   auto b=f.bytes();if(actual==3||actual==4||actual==5){++entry_coverage["final_state_"+std::to_string((b.u32(0x2f0)>>2)&31u)];if(actual==5)++entry_coverage[original?"crush_entered":"crush_rejected"];}
   if(actual==3||actual==4){auto ob=Bytes(before.data(),before.size());++entry_coverage["entry_feedback_mode_"+std::to_string(b.u8(0xc36)==0?0:1)];++entry_coverage["entry_reverse_"+std::to_string(ob.u32(0x59ac)&7u)];if((ob.u32(4)&1u)&&!ob.u32(0x59b0))++entry_coverage["bit0_set_wrecker_not_requested"];if(ob.u32(0x59a8)==1&&((b.u32(0x2f0)>>2)&31u)==5)++entry_coverage["entry_rerouted_1_to_5"];}
   if(actual==5){auto ob=Bytes(before.data(),before.size());if(ob.f32(0x2c8)>0)++entry_coverage["gate_existing_crash"];else if(ob.f32(0x2f8)>0)++entry_coverage["gate_recovery"];else if(ob.u32(0xdf8))++entry_coverage["gate_event_df8"];else if(!original)++entry_coverage["gate_speed_after_timer"];}
   if(actual==2)++entry_coverage[b.f32(0xbd0)==0?"feedback_zero":b.f32(0xbd0)==1?"feedback_one":"feedback_other"];
  }
 }
}
