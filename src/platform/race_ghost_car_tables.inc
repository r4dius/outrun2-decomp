// EXE .data 650500: 4866C0 car records (0x44 each, +0 = LOD list pointer).
OR2_EXE_BYTES(Exe650500,0x650500u,0x7F8u);
// EXE .rdata 5BC910..5C09C0: other-car LOD lists and descriptors (4ADBC0..4AE0B0).
OR2_EXE_BYTES(Exe5bc910,0x5BC910u,0x40B0u);
// EXE .data 683980: colour record pointers by car byte +11.
OR2_EXE_BYTES(Exe683980,0x683980u,0x78u);
// EXE .rdata 5C45F8/5C460C: lamp object ids (4AD7E0/4AD880).
OR2_EXE_BYTES(Exe5c45f8,0x5C45F8u,0x138u);
