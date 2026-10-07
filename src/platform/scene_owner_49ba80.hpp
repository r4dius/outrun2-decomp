#pragma once
// PC 0x49BA80: the START scene owner, a state machine on 836718 (stages
// 5..63, jump table 49C688) that requests and waits for the race resources.
// Every external PC function is reached through one service, called with the
// original entry point, the EAX argument (for the register-argument callees
// 4999A0, 49B390, 49B3C0, 49B3F0, 49B420) and the stack arguments, in the
// original order; its return value is the original EAX.
#include <array>
#include <cstdint>
#include <functional>
namespace outrun::platform {
struct SceneOwnerInputs {
    std::uint32_t variant_780258{};
    std::uint32_t preset_78024c{};
    // 7D3188 (selected course copy pointer): present, [[+0x18]+0x18], [[+0x18]+0x1C], [+0x64].
    bool course_7d3188{};
    std::uint32_t course_18_18{},course_18_1c{},course_64{};
};
using SceneOwnerCall=std::function<std::uint32_t(std::uint32_t pc,std::uint32_t eax,const std::array<std::uint32_t,3>& args)>;
// Returns the original EAX (1 once the owner is ready at stage 56).
std::uint32_t scene_owner_49ba80(std::uint32_t& stage_836718,const SceneOwnerInputs&,const SceneOwnerCall&);
}
