#pragma once
#include "platform/loader_asset_pack.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace outrun::platform {

// r151: the PC installation is the source of truth.  This store resolves the
// same resource-id/mode pairs that the original async loader uses, but reads
// the retail files lazily from disk.  Generated renderer caches are separate
// and are never treated as game resources.
struct RetailAssetStore {
    std::string root;
    std::map<std::uint64_t,LoaderAssetRecord> loader_cache;
    std::map<std::string,std::vector<std::uint8_t>> path_cache;
    std::uint32_t file_reads{};
    std::uint64_t bytes_read{};
    std::uint32_t cache_hits{};
};

bool retail_asset_store_open(RetailAssetStore& store,const std::string& root,
                             std::string* error=nullptr);
bool retail_asset_store_is_game_root(const std::string& root);

const LoaderAssetRecord* retail_asset_lookup(RetailAssetStore& store,
                                              std::uint32_t resource_id,
                                              std::uint32_t request_mode,
                                              std::string* error=nullptr);

// guest_path is a PC path such as "\\OCP\\foo.sz".  The payload is read from
// the retail tree and cached by normalized relative path.  If inflate_sz is
// true, exactly one zlib stream is decoded with a 128 MiB output cap.
const std::vector<std::uint8_t>* retail_asset_guest_path(
    RetailAssetStore& store,const std::string& guest_path,bool inflate_sz,
    std::string* error=nullptr);

bool retail_asset_read_relative(RetailAssetStore& store,const std::string& relative,
                                std::vector<std::uint8_t>& bytes,
                                std::size_t max_bytes=128u*1024u*1024u,
                                std::string* error=nullptr);
bool retail_asset_read_relative_inflated(RetailAssetStore& store,
                                         const std::string& relative,
                                         std::vector<std::uint8_t>& bytes,
                                         std::size_t max_bytes=128u*1024u*1024u,
                                         std::string* error=nullptr);
// One complete zlib stream (a retail .sz archive) with a 128 MiB cap.
bool retail_asset_inflate_sz(const std::vector<std::uint8_t>& input,std::vector<std::uint8_t>& output,
                             std::string* error=nullptr);
std::string retail_asset_path(const RetailAssetStore& store,
                              const std::string& relative);

} // namespace outrun::platform
