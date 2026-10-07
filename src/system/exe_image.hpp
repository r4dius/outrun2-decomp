#pragma once
// The player's own OR2006C2C.EXE (decompressed Steam build), mapped at its PC
// addresses. Nothing of the original executable is compiled into the port:
// the data the port reads by PC address comes from this image, which the
// first launch (or tools/or2setup) builds from the player's file and keeps in
// a cache next to the game.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "system/sha256.hpp"
namespace outrun::platform {
constexpr std::uint32_t ExeImageBase=0x00400000u;
constexpr std::uint32_t ExeImageSize=0x00590000u;
// Every PC range the port reads, with the SHA-256 it must have in the
// reference build (system/exe_image_ranges.inc, tools/exe_image_ranges.py).
struct ExeImageRange{std::uint32_t va,size;Sha256Digest sha256;};
extern const ExeImageRange ExeImageRanges[];
extern const std::size_t ExeImageRangeCount;
// Identity of the range table above; a cache verified against another table
// is checked again (from the cache alone) before use.
Sha256Digest exe_image_range_set_id();

bool exe_image_loaded();
// Pointer to [va, va+size) of the loaded image. Throws std::runtime_error
// when no image is loaded or the range is outside the image.
const std::uint8_t* exe_image_bytes(std::uint32_t va,std::uint32_t size);
std::uint32_t exe_image_u32(std::uint32_t va);

// Progress of a long step: done/total units and a short label.
using ExeImageProgress=std::function<void(std::uint64_t done,std::uint64_t total)>;

// Maps a PE file (sections at their virtual addresses) into a full image.
bool exe_image_map_pe(const std::vector<std::uint8_t>& file,std::vector<std::uint8_t>& image,std::string* error);
// Checks every range of ExeImageRanges; on failure *bad is the first range
// that differs. A compressed (packed) EXE fails here.
bool exe_image_verify(const std::vector<std::uint8_t>& image,std::string* error,const ExeImageProgress& progress={},std::size_t* bad=nullptr);
// Installs a verified image as the process image and binds the tables.
void exe_image_install(std::vector<std::uint8_t>&& image);

// Cache file (zlib-compressed image + header). load returns false with
// *error set when the file is missing, damaged or from another build and
// cannot be verified again.
// File name of the prepared image in the cache folder (exe_image.bin before 2026-10-06).
inline constexpr const char* ExeImageCacheName="OR2006C2C.cache";
bool exe_image_load_cache(const std::string& path,std::string* error);
bool exe_image_write_cache(const std::string& path,const std::vector<std::uint8_t>& image,std::string* error);

// What exe_image_find_source found in the game folder.
enum class ExeSource{Found,Compressed,OtherVersion,Missing};
// Looks in dir for a file that maps to a verified image (any *.exe name; the
// Steam OR2006C2C.EXE itself where the platform extracts it in memory, see
// exe_image_map_pe). Fills image and path on success; *found says what was
// there otherwise: only a compressed Steam OR2006C2C.EXE this build cannot
// read, an OR2006C2C.EXE of another edition, or none.
bool exe_image_find_source(const std::string& dir,std::vector<std::uint8_t>& image,std::string& path,
                           ExeSource* found,std::string* error,const ExeImageProgress& progress={});

// EXE-owned table copied out of the image when one is installed (the array
// lives in .bss: zero in the binary). Declare tables with OR2_EXE_BYTES so
// tools/exe_image_ranges.py finds their ranges.
struct ExeCopy{ExeCopy(std::uint8_t* destination,std::uint32_t va,std::uint32_t size);};
#define OR2_EXE_BYTES(name,va,size) alignas(4) static std::uint8_t name[size]{}; OR2_EXE_COPY(name,va,size)
// Copies [va, va+size) of the image into an existing table (any type, .bss).
#define OR2_EXE_COPY(table,va,size) static const ::outrun::platform::ExeCopy OR2_EXE_CONCAT(or2_exe_copy_,__COUNTER__){reinterpret_cast<std::uint8_t*>(&(table)),va,size}
// Calls fn whenever an image is installed (at once when one is loaded):
// tables the port builds from the image (see pc_sound_tables.inc).
struct ExeBind{explicit ExeBind(void (*fn)());};
#define OR2_EXE_BIND(fn) static const ::outrun::platform::ExeBind OR2_EXE_CONCAT(or2_exe_bind_,__COUNTER__){fn}
#define OR2_EXE_CONCAT2(a,b) a##b
#define OR2_EXE_CONCAT(a,b) OR2_EXE_CONCAT2(a,b)

// Development: with OR2_EXE_TRACE=<file>, every range read through
// exe_image_bytes is appended to <file> at exit (tools/exe_image_ranges.py
// --trace adds them to the checked ranges).
// Development builds and tests: loads the image from the file named by the
// OR2_EXE environment variable (a decompressed EXE or a cache file) when no
// image is loaded yet. Returns true when an image is loaded afterwards.
bool exe_image_autoload();
}
