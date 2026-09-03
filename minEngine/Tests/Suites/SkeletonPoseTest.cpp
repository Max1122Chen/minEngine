#include "Runtime/Function/Animation/AnimationConstants.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Core/Math/Math.h"

#include "doctest.h"

#include <cmath>
#include <string>
#include <vector>

namespace
{
    bool MatrixNear(const Matrix4& a, const Matrix4& b, float epsilon = 1e-4f)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (std::abs(a[column][row] - b[column][row]) > epsilon)
                {
                    return false;
                }
            }
        }
        return true;
    }

    minEngine::Skeleton MakeTwoBoneChain()
    {
        using namespace minEngine;

        std::vector<SkeletonBone> bones(2);
        bones[0].Name = "Root";
        bones[0].ParentIndex = -1;
        bones[0].LocalBind = Transform{};
        bones[0].InverseBindPose = Matrix4(1.0f);

        bones[1].Name = "Child";
        bones[1].ParentIndex = 0;
        bones[1].LocalBind.Position = Vector3(0.0f, 1.0f, 0.0f);
        bones[1].InverseBindPose = Matrix4(1.0f);

        Skeleton skeleton;
        std::string error;
        REQUIRE(skeleton.SetBones(std::move(bones), &error));
        return skeleton;
    }
}

TEST_CASE("skeleton-pose: fill bind pose and two-bone global [full]")
{
    using namespace minEngine;

    Skeleton skeleton = MakeTwoBoneChain();
    Pose pose;
    skeleton.FillBindPose(pose);
    REQUIRE(pose.GetBoneCount() == 2);
    CHECK(pose.At(1).Position.y == doctest::Approx(1.0f));

    std::vector<Matrix4> globalPose;
    skeleton.LocalToGlobal(pose, globalPose);
    REQUIRE(globalPose.size() == 2);

    const Matrix4 expectedChild =
        Transform{Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f)}.ToMatrix();
    CHECK(MatrixNear(globalPose[0], Matrix4(1.0f)));
    CHECK(MatrixNear(globalPose[1], expectedChild));
    CHECK(skeleton.FindBoneIndex("Child") == 1);
    CHECK(skeleton.FindBoneIndex("Missing") == -1);
}

TEST_CASE("skeleton-pose: identity inverse-bind palette equals global [full]")
{
    using namespace minEngine;

    Skeleton skeleton = MakeTwoBoneChain();
    Pose pose;
    skeleton.FillBindPose(pose);

    pose.At(1).Position = Vector3(0.0f, 2.0f, 0.0f);

    std::vector<Matrix4> globalPose;
    skeleton.LocalToGlobal(pose, globalPose);

    std::vector<Matrix4> palette;
    skeleton.BuildSkinningPalette(pose, palette);
    REQUIRE(palette.size() == globalPose.size());
    CHECK(MatrixNear(palette[0], globalPose[0]));
    CHECK(MatrixNear(palette[1], globalPose[1]));
}

TEST_CASE("skeleton-pose: reject parent-after-child ordering [full]")
{
    using namespace minEngine;

    std::vector<SkeletonBone> bones(2);
    bones[0].Name = "ChildFirst";
    bones[0].ParentIndex = 1;
    bones[1].Name = "ParentSecond";
    bones[1].ParentIndex = -1;

    Skeleton skeleton;
    std::string error;
    CHECK_FALSE(skeleton.SetBones(std::move(bones), &error));
    CHECK_FALSE(error.empty());
}

TEST_CASE("skeleton-pose: constants [full]")
{
    CHECK(minEngine::kMaxBoneInfluences == 4);
    CHECK(minEngine::kMaxBonesPerSkeleton == 256);
}
