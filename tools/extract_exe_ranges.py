#!/usr/bin/env python3
"""Emit src/platform/embedded_exe_ranges.cpp: the read-only ranges of
OR2006C2C.EXE addressed by their PC virtual address (EXE-owned tables consumed
by ported code through PcAddressView). The file lists addresses only; the bytes
come from the player's EXE at run time (system/exe_image). After adding a range,
regenerate this file and src/system/exe_image_ranges.inc:
    python3 tools/extract_exe_ranges.py src/platform/embedded_exe_ranges.cpp
    python3 tools/exe_image_ranges.py <decompressed Steam EXE> src/system/exe_image_ranges.inc
"""
import hashlib, struct, sys

# The base is the unpacked Steam build (7c4e8ec6...); the protected OR2006C2C.EXE
# (bdafa88a...) is the same build with the same data and is still accepted.
STEAM_SHA256 = '7c4e8ec6fcc54e04bfd82e6165e789dfe462e898dcde6f1107d7cc72799fa93d'
EXE_SHA256 = 'bdafa88a5abdd2a9743f6bdcc5e2189288c0203790412fce10b9bb649933482f'
ACCEPTED_SHA256 = (STEAM_SHA256, EXE_SHA256)
# (base, end, purpose)
RANGES = [
    (0x5C5200, 0x5C6400, 'name entry 4B2370..4B4F20 tables: row positions 5C5210, tokens 5C5458 / 5C5548 / 5C5C20 / 5C5C98 / 5C5D18 / 5C6284 / 5C62AC, layouts 5C5EF8 / 5C6028, charset 5C62E0, digits 5C6344, strings 5C633C..5C63B0, 5C6374'),
    (0x5A4600, 0x5A7100, 'arcade ending tables: 5A4618 motion sets, 5A4624 / 5A464C / 5A4674 ending ids, 5A46A0, 5A4740 background rows, 5A4824 + id * 0x4C rows (5A4830 scene, 5A4838 resources, 5A4874 4F2020, 5A4878 car), light blocks 5A5378.. and their list 5A6AB8'),
    (0x638DF0, 0x638F70, 'arcade ending .data: 638DF0 sound ids per ending, 638E98 ending id, 638E9C column, 638EA0 handle (writable copy in PcRaceEndState)'),
    (0x5A7780, 0x5A7830, 'arcade ending stage model names (452340: 5A46A4 rows -> END_*_STAGE / END_5x_S...)'),
    (0x5C1B70, 0x5C1BF0, 'name entry bad-word strings (68694C list, stored +1 per character)'),
    (0x68694C, 0x68697C, 'name entry bad-word list 68694C (.data pointers, 0-terminated)'),
    (0x5C6900, 0x5C6A90, 'name entry rank digit offset 5C6900 / 5C6904; AUTOSCENE HAKO_HUTA 5C6908, _USA 5C6914, car part / eye / KEM names 5C691C..5C6A8C'),
    (0x717D68, 0x718050, 'AUTOSCENE script rows 717D68 + scene * 12 (scenes 0..0x3D)'),
    (0x5E6B00, 0x5E6F50, '5E6B60 ("adding player %s to start packet", 516xxx), AUTOSCENE script paths (AS folder as_*_bin.sz)'),
    (0x5C09C0, 0x5C09C4, 'AUTOSCENE fov factor 5C09C0'),
    (0x687000, 0x6870D0, 'AUTOSCENE sound ids 687030 (scene events 2 / 4) and motion car letters 6870CC'),
    (0x626460, 0x626470, 'name entry "%s" 626468 and " " 62646C'),
    (0x5B3A30, 0x5B3CC0, '47CF40 rival name/param tables, class byte lists 5B3B70, 5B3C34/5B3C74 pairs'),
    (0x5B4460, 0x5B4480, '47CF40 "%s %d" and float constants'),
    (0x5E57D8, 0x5E5830, 'rival names (505390 memcpy of 32 bytes)'),
    (0x624C40, 0x624C60, 'rival name (505390 memcpy of 32 bytes)'),
    (0x64DE00, 0x64E200, 'rival configs 64DFF0/64E040, 64DEFC, 64DF68/64DF94/64DFA0, class table 64E090'),
    (0x6AF250, 0x6AF300, '505390 name pointers, 505340 table 6AF268'),
    (0x5C21E0, 0x5C2250, 'Heart Attack result sheet request result tokens 5C21E8'),
    (0x5C68E0, 0x5C6900, '4F2DF0 drift score factor 5C68E0'),
    (0x5C2250, 0x5C2380, '49B870 model family 5C2350 and resource table 5C2258'),
    (0x5CEE80, 0x5DA040, 'ORC78TBL primary (6A54E0) and secondary (6A55E8) course descriptors, 44DBB0 table 5D4FD8, descriptor file paths'),
    (0x654500, 0x654C00, 'robots .data: 488090 records 654868, 487EE0 table 6549B0; the passenger (489A90): 45BF30 motion triples 6545C8 / 654628, bone values 654754 / 654758, 487F50 charas 6549C8, voice records 6549E0 / 654A30 / 654B10'),
    (0x633558, 0x633DE8, '448AD0 model resource path table (0x223 entries, ends at the 633DE4 cursor)'),
    (0x59DD00, 0x5A2800, 'model resource path strings'),
    (0x5DA040, 0x5E3040, 'crash poses/keys, crash tables 5E08E8/5E0988, wall material/crush/friction tables 5E0DE0..5E3030'),
    (0x5E3040, 0x5E5000, 'vehicle display records (car +2B4 -> 5E3170..) read by PART_EFC 41BC60 (41F5F0 backfire: record +1644)'),
    (0x6AB600, 0x6AC400, 'crash poses 16..19 and their keys'),
    (0x5C2570, 0x5C2F80, 'crash reroute ranges 5C2570 (stage*64)'),
    (0x5A42F4, 0x5A4310, '44CF00 stage-8 branch collision path immediate LRBK_PALM (course-prog)'),
    (0x5A4310, 0x5A4568, 'course object animation paths BK/ANIMS/oso_dyn_bo_*_bin.sz named by the 636xxx object kind records'),
    (0x5E61A0, 0x5E62F0, 'camera motion table 5E61D0 (513730/513740: record, duration) and its one-key channels 5E61A0/5E61B0'),
    (0x6BB3C8, 0x712788, 'camera motions of 5E61D0 (20 channels {count, keys} per scene) and their keys (486EF0)'),
    # rival racers / traffic (race_traffic.cpp): .rdata constants and the course appear source tables
    (0x599400, 0x599500, 'traffic .rdata constants (599428/599438/59943C FLT_MAX)'),
    (0x5A2900, 0x5A2A00, 'traffic .rdata constants (5A29DC/5A29E4/5A29EC)'),
    (0x5A4600, 0x5A4700, 'traffic .rdata constants (5A4608/5A460C)'),
    (0x5B0000, 0x5B0400, 'traffic .rdata constants (5B005C/5B0068, 5B018C Heart Attack distance class, 5B0158.. request score tables)'),
    (0x63EE68, 0x64DE00, 'Heart Attack car tables 63EE68 / 644C28 (45D710), 64A9E8 / 64AA00 sign tokens, 64AAA8 crate pieces, 64AAD8 escorts (.data, no writer)'),
    (0x64E200, 0x650500, 'Heart Attack car table 644C28 continued (.data, no writer)'),
    (0x6510F8, 0x651428, 'Heart Attack car table 644C28 end (.data, no writer)'),
    (0x5B0CB8, 0x5B2FE0, 'car model layouts 5B0CB8 (0x128 bytes each: shadow / part model ids) and their table 5B2F68[model] (46A560 car-select draw)'),
    (0x5B2FE0, 0x5B3A00, 'colour list entries {key, alternative bank ids} reached through car / object colour lists (race_colour_alt; 5B3118 in Heart Attack)'),
    (0x5B3A00, 0x5B3A30, 'traffic .rdata constants before the rival tables'),
    (0x5B3CC0, 0x5B4460, 'traffic .rdata tables and constants (5B3DE4 class lengths, 5B3DF0, 5B3E08 speed records, 5B436C..5B445C)'),
    (0x5B4480, 0x5B4600, 'traffic .rdata constants after 5B4480'),
    (0x5C4000, 0x5C4100, 'traffic .rdata table 5C4000'),
    (0x5C6FC0, 0x5C6FE0, '4F02D0 view bound 5C6FC8'),
    (0x5DA2C0, 0x5DA2E0, '4F02D0 view bound 5DA2D4'),
    (0x7162C0, 0x7162D0, '504E70 frame time 7162C4 (.data, no writer)'),
    (0x6AC880, 0x6AC9C0, '4F9450 collision tables 6AC898..6AC9B4 (.data, no writer)'),
    (0x5E0980, 0x5E0C80, '4F9450 class bytes 5E0990, SE table 5E0A78, 5E0AB8/5E0AC0, 45FB80 heart anchors 5E0AC8, 45D470 models 5E0C34 / 5E0C50'),
    (0x5A9000, 0x5AFE00, '5A9094 (network host session table), 4F6FE0 constant 5A923C, 45FB80 5A91B8, Heart Attack HUD token tables 5A93F0 / 5A9468 / 5A9484 / 5A94D0, quest request tables 5A94D8 / 5AC818, 5AFB58 cones, 5AFC4C..5AFD38, 5AFD2C'),
    (0x650CF8, 0x6510F8, 'traffic model records 30..44 of 650500 (4866C0), after the 30 player models'),
    (0x596400, 0x596800, 'DirectInput data: c_dfDIJoystick 59650C, c_dfDIKeyboard 596714, GUID_SysKeyboard 59672C, IID_IDirectInput8A 5967CC'),
    (0x61E600, 0x61E700, 'keyboard input device: name 61E678, default key map 61E6B8'),
    (0x624AC0, 0x624BF0, 'input device vtables 624AD8 (base) / 624B30 (joystick 402640) / 624B88 (keyboard 403660)'),
    (0x738B40, 0x738B90, 'joystick default button map 738B48 (.data, copied by 402640)'),
    (0x5CC000, 0x5CCC00, 'controls configuration (4D7E00..4D7FB0, 4D6A60): key name table 5CC200..5CC8C4, slider table id 5CC048, constants 5CCBD8..5CCBF8, title owner vtable 5CCBB4'),
    (0x5CCC00, 0x5CEE80, 'frontend screen vtables 5CCC00.. (network screens 5CD834 key 14, 5CD84C key 15, 5CDA88 key 22...) and their constants'),
    (0x5C9600, 0x5CC000, 'frontend screen vtables 5C9600.. (network screens 5C97FC key 11, 5C9920 key 12, 5C9A28 key 13...)'),
    (0x59DA00, 0x59DB00, 'frontend screen vtables 59DABC (key 17) / 59DAD4 (key 19)'),
    (0x5C1B00, 0x5C1D00, 'network screen vtables 5C1C2C (key 46) / 5C1C48 (key 26) and the session state vtables 5C1BE8 / 5C1C18'),
    (0x692B00, 0x692CC0, 'title options layout .data 692B28..692CA0 (rows 692C50, count 692C94, timer 692C98: writable copy in the input device layer)'),
    (0x6255D0, 0x6255E0, 'empty string 6255D5 (4D7E00 window title)'),
    (0x5C1700, 0x5C1900, '5C17E4 (network notice screen 15), title window vtables 5C1838 / 5C1888 / 5C18A0 / 5C18C8 and the 48CAE0 constant 5C1858'),
    (0x596000, 0x62A000, 'whole .rdata 596000..629FFF for the network module (its screens and widgets read constants, strings and vtables everywhere)'),
    (0x59DB00, 0x59DCA0, '445DA0 UTF-16 character tables 59DB08 / 59DC08'),
    (0x5E9520, 0x600000, 'network module .rdata: Demonware vtables, constants and strings (5E9520..5FFFFF)'),
    (0x600000, 0x62A000, 'network module .rdata: Demonware / LAN constants, vtables and strings (600000..629FFF)'),
    (0x62A000, 0x780100, 'initialized .data image 62A000..7800FF: the network module starts from a private copy of the whole data region (its screens, sessions and widgets)'),
    (0x659900, 0x659B00, 'network module .data 659900..659AFF'),
    (0x65B500, 0x65B540, 'LAN sockets 65B51C / 65B520 / 65B524 (.data -1)'),
    (0x65D000, 0x65D010, 'network .data 65D008'),
    (0x65E000, 0x65E010, 'network .data 65E00C'),
    (0x6AA330, 0x6AA340, 'network .data 6AA334'),
    (0x724600, 0x724A00, 'network .data 7246A4..724934'),
    (0x733F00, 0x735800, 'network / CRT .data 733F40..735760'),
    (0x7F1930, 0x7F1940, 'network .data 7F1938'),
    (0x830B00, 0x830E00, 'network .data 830BEC..830D48'),
    (0x836000, 0x836300, 'network service object 83612C and 8360D4..'),
    (0x850B00, 0x850B40, 'network .data 850B04..850B34'),
    (0x85AF00, 0x85B400, 'LAN interface table 85AFB8 and 85B2B0'),
    (0x85DE00, 0x85E000, 'network .data 85DE90..85DF54'),
    (0x85EB00, 0x85EC00, 'network .data 85EB78..85EBE4'),
    (0x85F000, 0x85F100, 'network .data 85F0D0..85F0FC'),
    (0x85FA40, 0x85FA60, 'network .data 85FA50'),
    (0x988D00, 0x98AC00, 'network lobby 988F40, 98A628..98A6FC, 98AB04..98AB1C'),
    (0x751B00, 0x751C00, 'course shadow pass .data: 422820 light records 751B2C / 751B58 (copied, 751B94 / 751B98 written in the writable copy of race_course_passes)'),
    (0x5BA918, 0x5BAA00, 'camera .rdata after the 5BA4E0 table: presets 5BA918 / 5BA930, constants 5BA950..5BA96C, mode 10 eye 5BA980 / angles 5BA970 / fov 5BA97C'),
    (0x5BAA00, 0x5BC910, 'traffic model chains / model data of models 30..43 (before 5BC910)'),
    (0x5E8A88, 0x5E9520, '520950 collision descriptors 5E8A88 + model*0x3C (models 0..44)'),
    (0x6839F8, 0x683A30, '683980 colour list pointers of the traffic models 30..43 (.data, read only)'),
    (0x5DA300, 0x5DA360, '4F3EA0 default ranking records: 5DA318 leads, 5DA328/2C/30 names, 5DA334 nibbles'),
    (0x619A00, 0x619B00, '.rdata constants (619A34 = 0.0)'),
    (0x628000, 0x628400, '.rdata float constants 628000..6283FF'),
    (0x5C1C60, 0x5C1D50, 'course object .rdata constants 5C1C68 / 5C1D40'),
    (0x5C3700, 0x5C4000, 'course object shapes 5C37B8 / 5C3A30 / 5C3CA8, body tables 5C3F1C..5C3F94, constants 5C3FB0..5C3FF0'),
    (0x681200, 0x681E00, 'course object .data: ball shapes 6812D0 / 681548 (4A8EA0 rewrites 681548), kind table 6817C0, spark tables 681CAC / 681CE8, 681D24'),
    (0x6A54E0, 0x6A5700, 'course descriptor pointer tables 6A54E0 (primary) / 6A55E8 (secondary) (44C990: [6A54E0+i*4]+7E)'),
    (0x6A5DF8, 0x6A6DE8, 'course appear source tables (4EF890 copies to 84BD00) and stage pointers 6A6CE0'),
    # OUTRUN2SP arcade frontend (arcade_attract.cpp: event 4 functions 1..12, modules 0x11..0x13)
    (0x59DAE0, 0x59DB20, 'arcade .rdata string 59DAEC (sel_dl_edit0.tgt)'),
    (0x59DC80, 0x59DCC0, '446510 slot caption positions 59DC84 / 59DC88 / 59DCA4'),
    (0x5C2120, 0x5C21E0, 'arcade .rdata constants 5C2134 / 5C2138'),
    (0x5C6F00, 0x5C6FC0, 'arcade constants 5C6FB8 / 5C6FBC (player marks)'),
    (0x5C6FE0, 0x5C9600, 'arcade sprite table 5C7170 {token, first, last}, sequences 5C8BE0.., tables 5C91DC.., constants 5C9404..'),
    (0x625630, 0x625640, 'arcade .rdata constant 62563C'),
    (0x65A7A0, 0x65A7C0, 'arcade settings 65A7A4..65A7B3 (.data initial values; written copies live in PcRaceEndState)'),
    (0x6898C0, 0x68A000, 'arcade .data tables 6898C9..689FC8 (initial values; written copies live in PcRaceEndState)'),
    (0x687E20, 0x687E58, '4B88D0 / 4B8C00 player lamp x by player count 687E24 (.data, no writer)'),
]

def main():
    # Addresses only: the bytes come from the player's EXE (system/exe_image).
    out = sys.argv[-1]
    lines = ['// Generated by tools/extract_exe_ranges.py: PC ranges of OR2006C2C.EXE mapped by the ported code.',
             "// No data of the original: each range points into the player's EXE image once it is loaded.",
             '#include "platform/pc_address_view.hpp"', '#include "system/exe_image.hpp"',
             'namespace outrun::platform {', 'PcExeRange EmbeddedExeRanges[]={']
    for base, end, purpose in RANGES:
        lines.append('    {0x%08xu,nullptr,0x%xu}, // %s' % (base, end - base, purpose))
    lines.append('};')
    lines.append('const std::size_t EmbeddedExeRangeCount=%d;' % len(RANGES))
    lines.append('void bind_embedded_exe_ranges(){')
    lines.append('    for(auto& r:EmbeddedExeRanges)r.data=exe_image_bytes(r.base,static_cast<std::uint32_t>(r.size));')
    lines.append('}')
    lines.append('OR2_EXE_BIND(bind_embedded_exe_ranges);')
    lines.append('}')
    open(out, 'w', newline='\n').write('\n'.join(lines) + '\n')

if __name__ == '__main__':
    main()
