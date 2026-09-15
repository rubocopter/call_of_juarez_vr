#pragma once

#include <string>

struct ID3D11Texture2D;

namespace cojvr::runtime {
class OpenVrRuntime;
}

namespace cojvr::backends::openvr {

enum class EyeSubmission { Left, Right };

[[nodiscard]] bool SubmitEyeD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* texture,
    EyeSubmission eye,
    int& runtime_result,
    std::string& error) noexcept;

[[nodiscard]] bool SubmitStereoD3D11(
    runtime::OpenVrRuntime& runtime,
    ID3D11Texture2D* left_eye,
    ID3D11Texture2D* right_eye,
    std::string& error) noexcept;

} // namespace cojvr::backends::openvr
