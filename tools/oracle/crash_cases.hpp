// r020: direct public original PC entries, no crash/acos/keyframe callback.
constexpr auto CrashArena=outrun::testing::crash_guest_base;
bool crash_enabled(const std::string& only){if(only=="all"||only=="crash_all")return true;for(auto n:outrun::testing::crash_names)if(only==n)return true;return false;}
constexpr std::uint32_t crash_pages[]={0x5e0000,0x780000,0x89b000,0x956000,0x79f000,0x85f000};
constexpr unsigned crash_page_count=sizeof(crash_pages)/sizeof(crash_pages[0]);
constexpr std::uint32_t crash_mutable[][3]={{0x9563e8,0x4600,128},{0x9560c0,0x4680,4},{0x956124,0x4684,4}};
void init_crash_oracle(){map_at(CrashArena,0x10000,PROT_READ|PROT_WRITE);for(auto p:crash_pages)if(mprotect(reinterpret_cast<void*>(p),4096,PROT_READ|PROT_WRITE))throw std::runtime_error("crash globals mprotect");}
struct CrashBinding {
 std::array<std::array<std::uint8_t,4096>,crash_page_count> saved{},expected{};
 explicit CrashBinding(outrun::testing::CrashFixture& f){
  for(unsigned k=0;k<crash_page_count;++k)std::memcpy(saved[k].data(),reinterpret_cast<void*>(crash_pages[k]),4096);
  std::memcpy(reinterpret_cast<void*>(CrashArena),f.image.data(),f.image.size());auto b=f.bytes();
  auto put=[](std::uint32_t p,std::uint32_t v){*reinterpret_cast<std::uint32_t*>(p)=v;};
  const std::uint32_t bound[][3]={{0x5e0f00,0x4000,32},{0x5e0ee0,0x4040,32},{0x5e08e8,0x3000,160},{0x5e0988,0x3200,240}};
  for(const auto& m:bound)std::memcpy(reinterpret_cast<void*>(m[0]),f.image.data()+m[1],m[2]);
  for(const auto& m:crash_mutable)std::memcpy(reinterpret_cast<void*>(m[0]),f.image.data()+m[1],m[2]);
  for(unsigned t=0;t<4;++t){put(0x780140+4*t,(b.u32(0x4c60)&(1u<<t))?CrashArena+0x4800+t*16:0);put(0x780228+4*t,CrashArena+0x4a00+t*32);}
  put(0x89b564,CrashArena+0x1a00+b.u32(0x4c40));put(0x89b568,b.u32(0x4c44));put(0x89b56c,b.u32(0x4c48));
  *reinterpret_cast<std::uint8_t*>(0x79fcc7)=b.u8(0x4688);put(0x780258,b.u32(0x4c64));put(0x78024c,b.u32(0x4c68));put(0x85fa64,b.u32(0x4c6c));
  for(unsigned k=0;k<crash_page_count;++k)std::memcpy(expected[k].data(),reinterpret_cast<void*>(crash_pages[k]),4096);
 }
 void capture(){for(const auto& m:crash_mutable)std::memcpy(reinterpret_cast<void*>(CrashArena+m[1]),reinterpret_cast<void*>(m[0]),m[2]);}
 void compare(const std::string& name,outrun::testing::CrashFixture& f){
  compare_world_bytes(name+"_memory",reinterpret_cast<void*>(CrashArena),f.image.data(),f.image.size());
  for(const auto& m:crash_mutable){unsigned k=0;while(crash_pages[k]!=(m[0]&~4095u))++k;std::memcpy(expected[k].data()+(m[0]&4095u),f.image.data()+m[1],m[2]);}
  for(unsigned k=0;k<crash_page_count;++k)compare_world_bytes(name+"_globals_"+std::to_string(k),reinterpret_cast<void*>(crash_pages[k]),expected[k].data(),4096);
 }
 ~CrashBinding(){for(unsigned k=0;k<crash_page_count;++k)std::memcpy(reinterpret_cast<void*>(crash_pages[k]),saved[k].data(),4096);}
};
bool crash_fp_return(unsigned id){return id==0||id==4||id==6||id==8||id==9;}
std::uint32_t run_crash_original(unsigned id){
 const std::uint32_t entries[]={0x5034c0,0x503570,0x43d470,0x5035e0,0x5134c0,0x513590,0x513650,0x5136c0,0x4f6580,0x4f6590,0x4a2400};
 prepare(entries[id]);Bytes b(reinterpret_cast<void*>(CrashArena),outrun::testing::crash_image_size),st(reinterpret_cast<void*>(S),64);
 switch(id){
  case 0:guest_call.esi=CrashArena+0x4c00;st.put32(0,CrashArena+0x4c10);break;
  case 1:guest_call.ecx=CrashArena;guest_call.edx=CrashArena+0x1200;st.put32(0,CrashArena+0x4400);st.put32(4,b.u32(0x4c30));break;
  case 2:st.put32(0,b.u32(0x5c));break;
  case 3:guest_call.eax=CrashArena;st.put32(0,b.u32(0x4c30));break;
  case 4:st.put32(0,CrashArena+0x2200+b.u32(0x4c28)*16);st.put32(4,CrashArena+0x2200+b.u32(0x4c2c)*16);st.put32(8,b.u32(0x4c24));break;
  case 5:st.put32(0,CrashArena+0x2000);st.put32(4,b.u32(0x4c24));break;
  case 6:st.put32(0,CrashArena+0x2100);st.put32(4,b.u32(0x4c24));break;
  case 7:st.put32(0,CrashArena+0x2100);st.put32(4,b.u32(0x4c24));st.put32(8,CrashArena+b.u32(0x4c38));st.put32(12,CrashArena+b.u32(0x4c3c));break;
  case 8:case 9:st.put32(0,b.u32(0x4c34));break;
  case 10:st.put32(0,CrashArena);break;
 }
 guest_call.st0=crash_fp_return(id);run();return guest_call.st0?guest_call.out_st0:(id==2?guest_call.out_eax&0xffffu:0u);
}
std::ofstream crash_golden;std::uint32_t crash_golden_count=0;
void crash_u32(std::uint32_t v){char b[4];for(unsigned k=0;k<4;++k)b[k]=char(v>>(8*k));crash_golden.write(b,4);}
void begin_crash_golden(const char* path){crash_golden.open(path,std::ios::binary|std::ios::trunc);if(!crash_golden)throw std::runtime_error("crash snapshot open");crash_golden.write("OR2R020\0",8);crash_u32(2);crash_u32(x87_control);crash_u32(0);crash_u32(outrun::testing::crash_image_size);}
void finish_crash_golden(){if(crash_golden.is_open()){crash_golden.seekp(16);crash_u32(crash_golden_count);crash_golden.flush();if(!crash_golden)throw std::runtime_error("crash snapshot write");crash_golden.close();}}
std::map<std::string,std::uint64_t> crash_coverage;
void run_crash_cases(unsigned i,const std::string& only){
 if(!crash_enabled(only))return;
 WorldNativeControl control(static_cast<unsigned short>(x87_control));
 for(unsigned id=0;id<outrun::testing::crash_selector_count;++id){
  if(only!="all"&&only!="crash_all"&&only!=outrun::testing::crash_names[id])continue;
  auto f=outrun::testing::make_crash_fixture(i,id);CrashBinding binding(f);const auto before=f.image;
  const auto stages=outrun::testing::crash_stage_count(id);const bool capture=crash_golden.is_open()&&i<128;
  if(capture){crash_u32(id);crash_u32(i);crash_u32(stages);crash_golden.write(reinterpret_cast<const char*>(before.data()),before.size());++crash_golden_count;}
  for(unsigned stage=0;stage<stages;++stage){
   const auto actual=outrun::testing::crash_stage_id(id,stage);const auto old=f.image;auto ob=Bytes(const_cast<std::uint8_t*>(old.data()),old.size());
   const auto original=run_crash_original(actual);binding.capture();
   std::array<std::uint8_t,10> raw;std::memcpy(raw.data(),guest_call.out_st0_raw,10);
   if(capture){crash_u32(original);std::array<std::uint8_t,12> fp{};if(crash_fp_return(actual))std::memcpy(fp.data(),raw.data(),10);crash_golden.write(reinterpret_cast<const char*>(fp.data()),12);crash_golden.write(reinterpret_cast<const char*>(CrashArena),before.size());}
   long double ext=0;const auto native=outrun::testing::run_crash_native(f,actual,&ext);
   const auto name=std::string(outrun::testing::crash_names[id])+(stages>1?"_"+std::to_string(stage+1):"");
   const auto ext_key=name+"_st0_80bit";
   const auto ext_fail=[&](){const auto it=stats.find(ext_key);return it==stats.end()?std::uint64_t(0):it->second.fail;};
   const auto failures=stats[name+"_memory"].fail+stats[name+"_return"].fail+ext_fail();
   compare_u32(name+"_return",original,native);binding.compare(name,f);
   if(crash_fp_return(actual))compare_world_bytes(name+"_st0_80bit",raw.data(),&ext,10);
   if(stats[name+"_memory"].fail+stats[name+"_return"].fail+ext_fail()!=failures)std::cerr<<"crash case="<<i<<" selector="<<name<<"\n";
   auto b=f.bytes();
   if(actual==10){++crash_coverage[ob.f32(0x2c8)>0?(b.f32(0x2c8)>0?"crash_active":"crash_expired"):(ob.f32(0x2f8)>0?"recovery_tick":"idle")];if(b.f32(0x2c8)>0)++crash_coverage["orientation_mode_"+std::to_string((b.u32(0x2f0)>>7)&7)];}
   if(actual==1){++crash_coverage[std::memcmp(old.data()+0x4600,f.image.data()+0x4600,128)?"sound_queued":"sound_not_queued"];++crash_coverage[ob.i8(0xd23)>0?"sound_cooldown_gate":"sound_cooldown_set"];}
   if(actual==5)++crash_coverage["search_count_"+std::to_string(b.i16(0x2002))];
  }
 }
}
