#pragma once
#include "backends/d3d9/sampled_depth_capture.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace cojvr::games::call_of_juarez {
struct CoJSceneDepthPreviewIdentity {
    std::uint64_t frame=0, pose=0, owner=0, generation=0, sequence=0;
    unsigned eye=0, width=0, height=0;
};

// Producer-thread diagnostic only. A manifest is published last and its files
// are never overwritten. Missing manifests leave partial writes unqualified.
inline std::filesystem::path ExportCoJSceneDepthPreview(
    const std::filesystem::path& run_directory, std::string_view run_id,
    const CoJSceneDepthPreviewIdentity& id,
    const backends::d3d9::D3D9SampledDepthPreview& pixels) noexcept {
    try {
        if (run_id.empty() || run_id=="unbound" || run_id=="." || run_id==".." ||
            !std::all_of(run_id.begin(),run_id.end(),[](unsigned char c) {
                return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';
            }) || !id.frame || !id.pose || !id.owner || !id.generation || !id.sequence ||
            id.eye>1 || !id.width || !id.height || id.width>4096 || id.height>4096)
            return {};
        const auto extent=std::max(id.width,id.height);
        const auto limit=std::min(extent,backends::d3d9::D3D9SampledDepthPreview::max_extent);
        if (pixels.width!=std::max(1U,id.width*limit/extent) ||
            pixels.height!=std::max(1U,id.height*limit/extent) ||
            pixels.color_rgba16f.size()!=static_cast<std::size_t>(pixels.width)*pixels.height*4 ||
            pixels.depth_rgba16f.size()!=pixels.color_rgba16f.size()) return {};
        const auto directory=run_directory/"runtime"/"scene-depth";
        const auto stem="f"+std::to_string(id.frame)+"-p"+std::to_string(id.pose)+
            "-g"+std::to_string(id.generation)+"-e"+std::to_string(id.eye);
        const auto color=directory/(stem+".color.f16"), depth=directory/(stem+".depth.f16");
        const auto manifest=directory/(stem+".json");
        const auto temporary=[](std::filesystem::path file) { file+=".tmp"; return file; };
        for (const auto& file : {color,depth,manifest})
            if (std::filesystem::exists(file) || std::filesystem::exists(temporary(file))) return {};
        std::filesystem::create_directories(directory);
        const auto write=[&](const std::filesystem::path& path, const std::vector<std::uint16_t>& plane) {
            std::ofstream out(temporary(path),std::ios::binary);
            out.write(reinterpret_cast<const char*>(plane.data()),
                static_cast<std::streamsize>(plane.size()*sizeof(std::uint16_t)));
            out.close();
            return out.good();
        };
        if (!write(color,pixels.color_rgba16f) || !write(depth,pixels.depth_rgba16f)) return {};
        std::ofstream out(temporary(manifest),std::ios::binary);
        out << "{\"schemaVersion\":1,\"runId\":\"" << run_id
            << "\",\"frame_sequence\":" << id.frame << ",\"pose_sequence\":" << id.pose
            << ",\"owner\":" << id.owner << ",\"generation\":" << id.generation
            << ",\"capture_sequence\":" << id.sequence << ",\"eye\":" << id.eye
            << ",\"source_width\":" << id.width << ",\"source_height\":" << id.height
            << ",\"width\":" << pixels.width << ",\"height\":" << pixels.height
            << ",\"source_boundary\":\"1B_16\",\"sampling\":\"point\""
            << ",\"format\":\"rgba16f_le\",\"depth_encoding\":\"device_z_rgb24_integer_bytes\""
            << ",\"color_file\":\"" << color.filename().string()
            << "\",\"depth_file\":\"" << depth.filename().string()
            << "\",\"temporal_admitted\":false}\n";
        out.close();
        if (!out.good()) return {};
        std::filesystem::rename(temporary(color),color);
        std::filesystem::rename(temporary(depth),depth);
        std::filesystem::rename(temporary(manifest),manifest);
        return manifest;
    } catch (...) { return {}; }
}
}
