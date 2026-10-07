// OR2006C2C.EXE .rdata 5C4740..5C4C00 (route screen tables).
OR2_EXE_BYTES(Exe5c4740,0x5C4740u,0x4C0u);
// .rdata 5A7AB4..5A7ADC (453370 route key tree).
OR2_EXE_BYTES(Exe5a7ab4,0x5A7AB4u,0x28u);
// .rdata 5C2258..5C2350 (499BB0 model id -> resource pairs, -1 terminated).
OR2_EXE_BYTES(Exe5c2258,0x5C2258u,0xF8u);
// .data 651758..653788: goal/start camera scripts (0x2C-byte records ending with a
// negative time +24 / op 0x1F; +28 set by 4871A0; scene 31 ends at 65375C).
OR2_EXE_BYTES(Exe651758,0x651758u,0x2030u);
// .data 653788..653818: script of each camera scene.
OR2_EXE_BYTES(Exe653788,0x653788u,0x90u);
// .rdata 5C2370..5C23FC (4871A0 scene per preset/goal, 5C23D4/5C23E8 goal music).
OR2_EXE_BYTES(Exe5c2370,0x5C2370u,0x8Cu);
// .data 67F618..67F638 (499A30/49A060 START layout) and 67F644..67F6E0 (67F648 player icons, 67F6C0 rank icons).
OR2_EXE_BYTES(Exe67f618,0x67F618u,0x20u);
OR2_EXE_BYTES(Exe67f644,0x67F644u,0x9Cu);
// .rdata 5C1E00..5C2240 (GAD_PUB tables, 498670 / 499020 layouts and formats) and .data 67EE2C (GOAL voices).
OR2_EXE_BYTES(Exe5c1e00,0x5C1E00u,0x440u);
OR2_EXE_BYTES(Exe67ee2c,0x67EE2Cu,0xCu);
