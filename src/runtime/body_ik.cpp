#include "runtime/body_ik.hpp"

#include <algorithm>
#include <cmath>

namespace cojvr::runtime {
namespace {

bool Finite(const Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vec3 Add(const Vec3 left, const Vec3 right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

Vec3 Subtract(const Vec3 left, const Vec3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vec3 Scale(const Vec3 value, const float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const Vec3 left, const Vec3 right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 Cross(const Vec3 left, const Vec3 right) noexcept {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float Length(const Vec3 value) noexcept {
    return std::sqrt(Dot(value, value));
}

Vec3 Normalize(const Vec3 value, const Vec3 fallback = {}) noexcept {
    const float length = Length(value);
    if (!std::isfinite(length) || length <= 1.0e-5F) return fallback;
    return Scale(value, 1.0F / length);
}

Vec3 StablePerpendicular(const Vec3 direction) noexcept {
    const Vec3 axis = std::fabs(direction.y) < 0.9F
        ? Vec3{0.0F, 1.0F, 0.0F}
        : Vec3{1.0F, 0.0F, 0.0F};
    return Normalize(Cross(direction, axis), {0.0F, 0.0F, 1.0F});
}

} // namespace

TwoBoneIKResult SolveTwoBoneIK(
    const Vec3 root,
    const Vec3 target,
    const Vec3 pole_direction,
    const float upper_length,
    const float lower_length) noexcept {
    TwoBoneIKResult result{};
    result.root = root;
    result.upper_length = upper_length;
    result.lower_length = lower_length;
    if (!Finite(root) || !Finite(target) || !Finite(pole_direction) ||
        !std::isfinite(upper_length) || !std::isfinite(lower_length) ||
        upper_length <= 1.0e-4F || lower_length <= 1.0e-4F) {
        return result;
    }

    const Vec3 to_target = Subtract(target, root);
    const float target_distance = Length(to_target);
    if (!std::isfinite(target_distance) || target_distance <= 1.0e-4F) return result;
    const Vec3 direction = Scale(to_target, 1.0F / target_distance);

    const float minimum_reach = std::fabs(upper_length - lower_length) + 1.0e-4F;
    const float maximum_reach = upper_length + lower_length - 1.0e-4F;
    if (maximum_reach <= minimum_reach) return result;
    const float solved_distance = std::clamp(target_distance, minimum_reach, maximum_reach);
    result.target_clamped = std::fabs(solved_distance - target_distance) > 1.0e-4F;
    result.end = Add(root, Scale(direction, solved_distance));

    const float along =
        (upper_length * upper_length - lower_length * lower_length +
         solved_distance * solved_distance) /
        (2.0F * solved_distance);
    const float height_squared =
        std::max(0.0F, upper_length * upper_length - along * along);
    const float height = std::sqrt(height_squared);

    Vec3 bend = Subtract(pole_direction, Scale(direction, Dot(pole_direction, direction)));
    bend = Normalize(bend, StablePerpendicular(direction));
    result.joint = Add(Add(root, Scale(direction, along)), Scale(bend, height));
    result.valid = Finite(result.joint) && Finite(result.end);
    return result;
}

IKBodyPose BodyIKSolver::Solve(const BodyPose& body) const noexcept {
    IKBodyPose result{};

    // First-pass torso chain. A future full solver can replace these direct
    // anchors with constrained bones without changing the adapter contract.
    result.pelvis = body.pelvis;
    result.spine = body.pelvis;
    result.chest = body.head;
    result.head = body.head;
    if (body.chest.position_valid) {
        result.chest = body.chest;
    }
    if (body.pelvis.position_valid && body.head.position_valid) {
        // Keep a separate torso chain. The head target remains owned by the
        // HMD, while intermediate joints are generated in body space.
        result.spine.position.y += 0.35F;
        result.spine.position_valid = true;
        if (!body.chest.position_valid) {
            result.chest.position.x = (body.pelvis.position.x + body.head.position.x) * 0.5F;
            result.chest.position.y = body.pelvis.position.y + 0.55F;
            result.chest.position.z = (body.pelvis.position.z + body.head.position.z) * 0.5F;
        }
        result.chest.position_valid = true;
    }

    if (body.left_hand.position_valid) {
        result.left_arm = body.left_hand;
    }
    if (body.right_hand.position_valid) {
        result.right_arm = body.right_hand;
    }

    // Initial lower-body anchors. Until the game exposes its capsule and
    // skeleton constraints, keep feet in player space and derive a simple leg
    // chain instead of moving the camera/body together.
    result.left_foot = body.left_foot;
    result.right_foot = body.right_foot;
    if (body.pelvis.position_valid) {
        result.left_leg = body.pelvis;
        result.right_leg = body.pelvis;
        result.left_leg.position.x -= 0.12F;
        result.right_leg.position.x += 0.12F;
        result.left_leg.position.y -= 0.45F;
        result.right_leg.position.y -= 0.45F;
        result.left_leg.position_valid = true;
        result.right_leg.position_valid = true;
    }

    return result;
}

} // namespace cojvr::runtime
