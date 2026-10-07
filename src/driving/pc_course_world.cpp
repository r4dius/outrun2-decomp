#include "driving/pc_course_world.hpp"
#include <array>
#include <limits>
#include <stdexcept>
namespace outrun::driving {
namespace {
// A query may touch the active slot (overflow) and/or its next slot. Rollback
// only on a native error, NEVER on an ordinary original no-hit result.
struct MatrixRollback {
    PcMatrixStack& stack;
    std::ptrdiff_t offset;
    std::int32_t depth;
    std::array<std::array<std::uint8_t,64>,2> saved{};
    std::array<std::size_t,2> positions{};
    std::array<bool,2> present{};
    bool commit=false;
    explicit MatrixRollback(PcMatrixStack& s):stack(s),offset(s.current_offset),depth(s.depth){
        for(unsigned k=0;k<2;++k){
            const auto delta=std::ptrdiff_t(k*64u);
            if(offset>std::numeric_limits<std::ptrdiff_t>::max()-delta)continue;
            const auto o=offset+delta;
            if(o>=0&&static_cast<std::size_t>(o)<=s.storage.size()&&s.storage.size()-static_cast<std::size_t>(o)>=64){
                positions[k]=static_cast<std::size_t>(o);
                const auto b=s.storage.sub(positions[k],64);
                for(unsigned j=0;j<64;++j)saved[k][j]=b.u8(j);
                present[k]=true;
            }
        }
    }
    ~MatrixRollback(){
        if(commit)return;
        for(unsigned k=0;k<2;++k)if(present[k]){
            auto b=stack.storage.sub(positions[k],64);
            for(unsigned j=0;j<64;++j)b.put8(j,saved[k][j]);
        }
        stack.current_offset=offset;stack.depth=depth;
    }
};
}
namespace {
std::uint32_t get_y_position_prog_types(CourseWorldQuery& q,std::uint32_t mode,CourseProbe& point,
                                        std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind,
                                        std::uint32_t first,std::uint32_t second,std::uint32_t failure_return,
                                        bool predict=true,float miss_y=-0.1f){
    if(first>=4u||second>=4u)throw std::out_of_range("selected course type outside four explicit tables");
    MatrixRollback guard(q.matrices);pc_matrix_push(q.matrices);
    CourseProbe local{};decltype(get_road_cond(q.tables.courses[0],0,mode,0.f,0.f,0.f,q.tables.tuning)) result{};bool hit=false;
    std::uint32_t type=first;
    for(unsigned attempt=0;attempt<2;++attempt){
        type=attempt==0u?first:second;
        if(q.tables.grids_present[type]){
            const auto& course=q.tables.courses[type];
            if(course.load_type!=type)throw std::invalid_argument("selected course type/view mismatch");
            pc_matrix_load(q.matrices,q.tables.transforms[type==0u?0u:1u]);
            local=pc_matrix_inverse_point(q.matrices,point);
            const auto cell=calc_collision_area(local.x,local.z);
            const auto list=static_cast<std::uint16_t>(q.tables.grids[type].i16(static_cast<std::size_t>(cell)*2u));
            result=get_road_cond(course,list,mode,local.x,local.y,local.z,q.tables.tuning);
            local.y=result.y;
            if(result.polygon>=0&&list!=0u){hit=true;break;}
        }
    }
    auto predicted=q.prediction;std::uint32_t classification=1u;float world_y=miss_y;
    if(hit){
        if(predict)update_easy_lct_prediction_table(predicted,type);
        classification=1u<<(q.tables.courses[type].kinds.u8(static_cast<std::size_t>(result.polygon))&31u);
        world_y=pc_matrix_point(q.matrices,local).y;
    }
    pc_matrix_pop(q.matrices);
    point.y=world_y;
    if(polygon)*polygon=hit?static_cast<std::uint32_t>(result.polygon):0xffffffffu;
    if(kind)*kind=classification;
    if(special&&hit&&(classification&0xf00002u))*special=static_cast<std::uint32_t>(result.polygon);
    q.prediction=predicted;guard.commit=true;return hit?type:failure_return;
}
}
std::uint32_t get_y_position_prog(CourseWorldQuery& q,std::uint32_t mode,CourseProbe& point,
                                  std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind){
    const auto first=q.prediction.easy;
    if(first>=4u)throw std::out_of_range("initial predicted course type outside four explicit tables");
    const auto second=first==0u?1u:0u;
    return get_y_position_prog_types(q,mode,point,polygon,special,kind,first,second,second);
}
std::uint32_t get_y_position_prog_bk(CourseWorldQuery& q,const PcBkQueryContext& c,
                                     std::uint32_t mode,CourseProbe& point,
                                     std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind){
    const std::uint32_t course_gate=c.game_mode==4u?c.mode4_course_gate:q.prediction.easy;
    bool active=course_gate!=0u && c.branch_record!=2u;
    if(active && c.game_mode!=3u && c.game_mode!=4u)
        active=c.gate_4957f0||c.gate_48b310||c.gate_495490;
    if(!active)return get_y_position_prog(q,mode,point,polygon,special,kind);
    const std::uint32_t first=c.branch_record==1u?3u:2u;
    // The special path retries only primary type 0 and returns zero on a miss,
    // unlike ordinary GetYPositionProg which returns its second selected type.
    return get_y_position_prog_types(q,mode,point,polygon,special,kind,first,0u,0u);
}
std::uint32_t get_y_position_43ed20(CourseWorldQuery& q,std::uint32_t first,std::uint32_t mode,CourseProbe& point,
                                   std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind,float miss_y){
    const std::uint32_t second=first==0u?1u:0u;
    return get_y_position_prog_types(q,mode,point,polygon,special,kind,first,second,second,false,miss_y);
}
std::uint32_t get_y_position_branch_43f110(CourseWorldQuery& q,std::uint32_t branch_kind,
                                          std::uint32_t mode,CourseProbe& point,
                                          std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind){
    if(branch_kind==2u)return get_y_position_prog(q,mode,point,polygon,special,kind);
    return get_y_position_prog_types(q,mode,point,polygon,special,kind,branch_kind==1u?3u:2u,0u,0u);
}
std::uint32_t get_y_position_spl_chk(CourseWorldQuery& q,CourseProbe& point,
                                     std::uint32_t* polygon,std::uint32_t* special,std::uint32_t* kind){
    const auto original=point;CourseProbe probe=original;
    std::uint32_t index=0xffffffffu,flags=1u,type=0u;
    for(unsigned attempt=0;attempt<4;++attempt){
        probe=original;
        if(attempt&1u)probe.x=static_cast<float>(original.x+0.001f);
        if(attempt&2u)probe.z=static_cast<float>(original.z+0.001f);
        type=get_y_position_prog(q,0x400u,probe,&index,nullptr,&flags);
        if(!(flags&1u))break;
    }
    point.y=probe.y;
    if(polygon)*polygon=index;
    if(kind)*kind=flags;
    if(special&&(flags&0xf00002u))*special=index;
    return type;
}
}
