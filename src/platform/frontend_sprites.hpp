#pragma once
#include "platform/game_ui_pack.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace outrun::platform {
// PC 427F70/4282B0: 21 layers, 64 reusable slots in each layer. Handles are
// layer*64+slot, including zero; they are not guest pointers or unique tokens.
struct FrontendSprite {
    bool allocated{}, visible{true};
    std::uint32_t token{}, mode{}, status{}, pause_domain{};
    float frame{}, first{}, last{}, speed{1.0f}, rate{};
    float canvas_width{640.0f},canvas_height{480.0f};
    std::array<float,16> matrix{};
    std::int32_t id_2c{-1};      // +0x2C, copied to 7551B4 by 428170 (alloc sets -1)
};
struct FrontendSpriteTiming { float frames{}, rate{},width{640.0f},height{480.0f},root4{}; };   // root4: root component +4 (429780)
struct FrontendSpriteClock {
    bool sixty_hz{true}, advance_fifty_hz{true}, paused{};
};
class FrontendSprites {
public:
    static constexpr unsigned Layers=21, Slots=64, Count=Layers*Slots;
    // Copies only the last/root component's duration and frame rate (48BD20),
    // never the textures. Invalid archives leave the previous bank intact.
    bool bind(std::uint32_t bank,const GameUiPack& pack);
    bool bind_timing(std::uint32_t bank,const std::vector<FrontendSpriteTiming>& scenes);
    void reset(); // Keeps the loaded banks, like resetting the PC instance pool.
    // 427E30/428600 pool part: zero every layer count and next-slot cursor and
    // only the +0 allocated word of each instance; all other instance fields
    // (status, token, frame...) keep their stale values like the PC pool.
    void clear_allocations();
    std::uint32_t create(std::uint32_t token,std::uint32_t layer,std::uint32_t mode,
                         std::int32_t first=-1,std::int32_t last=-1,
                         bool explicit_range=false,std::uint32_t pause_domain=0);
    // 4285A0: clears +0 even when already free, decrements a positive layer
    // count and moves the layer cursor to the slot. Returns the old +0 state.
    bool release(std::uint32_t handle);
    bool set_speed(std::uint32_t handle,float speed);
    bool set_frame(std::uint32_t handle,std::int32_t frame);
    bool set_matrix(std::uint32_t handle,const std::array<float,16>& matrix);
    std::uint32_t status(std::uint32_t handle) const;
    const FrontendSprite* get(std::uint32_t handle) const;
    void tick(const FrontendSpriteClock& clock={});
    void drawn(std::uint32_t handle); // 428170: mode 4 lives for one draw.
    const std::array<FrontendSprite,Count>& instances() const {return instances_;}
    FrontendSprite* get_mutable(std::uint32_t handle){return handle<Count?&instances_[handle]:nullptr;}
    unsigned next(unsigned layer) const {return layer<Layers?next_[layer]:0;}
    bool bank_scene(std::uint32_t token,FrontendSpriteTiming& out) const;
    unsigned used(unsigned layer) const {return layer<Layers?used_[layer]:0;}
private:
    std::array<std::vector<FrontendSpriteTiming>,0x4b-0x20> banks_;
    std::array<FrontendSprite,Count> instances_{};
    std::array<unsigned,Layers> used_{},next_{};
};
// Transform the evaluated authored pose; the scene pack uses a top-left origin
// while PC instance matrices use scene-centred coordinates.
void frontend_sprite_transform(GameUiDraw& draw,const FrontendSprite& sprite,
                               float width,float height);
}
