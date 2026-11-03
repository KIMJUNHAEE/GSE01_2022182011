#include "stdafx.h"
#include "ModelRenderer.h"
#include "Renderer.h"
#include <set>
#include <stdexcept>

namespace
{
    void Check(bool ok, const char* message)
    {
        if (!ok)
        {
            throw std::runtime_error(message);
        }
    }

    void VerifySampling()
    {
        auto asset = std::make_shared<ModelAsset>();
        asset->nodes.resize(2);
        asset->nodes[1].parent = 0;
        asset->nodes[1].translation = {0, 1, 0};
        asset->hierarchyOrder = {0, 1};
        ModelChannel channel;
        channel.node = 0;
        channel.path = ModelChannelPath::Translation;
        channel.interpolation = ModelInterpolation::Linear;
        channel.times = {0, 1};
        channel.values = {{0, 0, 0, 0}, {10, 0, 0, 0}};
        ModelClip clip;
        clip.name = "Linear";
        clip.duration = 1;
        clip.channels = {channel};
        asset->clips.push_back(clip);
        clip.name = "Step";
        clip.channels[0].interpolation = ModelInterpolation::Step;
        asset->clips.push_back(clip);
        clip.name = "Cubic";
        clip.channels[0].interpolation = ModelInterpolation::CubicSpline;
        clip.channels[0].values = {{}, {}, {2, 0, 0, 0}, {}, {10, 0, 0, 0}, {}};
        asset->clips.push_back(clip);
        ModelInstance first(asset), second(asset);
        Check(first.Play("Linear", false), "Cannot select animation");
        first.Seek(.5f);
        auto point = TransformPoint(first.WorldMatrices()[1], {});
        Check(std::abs(point.x - 5) < .0001f && std::abs(point.y - 1) < .0001f,
              "TRS hierarchy/linear sampling failed");
        second.Play("Linear", false);
        second.Seek(.8f);
        Check(std::abs(first.WorldMatrices()[0].values[12] - 5) < .0001f,
              "Instances share animation state");
        first.Play("Step", false);
        first.Seek(.5f);
        Check(std::abs(first.WorldMatrices()[0].values[12]) < .0001f, "STEP interpolation failed");
        first.Play("Cubic", false);
        first.Seek(.5f);
        Check(std::abs(first.WorldMatrices()[0].values[12] - 5.25f) < .0001f,
              "Cubic tangent scaling failed");
        first.Play("Linear", true);
        first.Seek(1.25f);
        Check(std::abs(first.WorldMatrices()[0].values[12] - 2.5f) < .0001f,
              "Animation loop failed");
        first.Play("Linear", false);
        first.Seek(10);
        Check(std::abs(first.WorldMatrices()[0].values[12] - 10) < .0001f,
              "One-shot did not clamp");
        auto rotation = Slerp({0, 0, 0, 1}, {0, 1, 0, 0}, .5f);
        point = TransformPoint(ModelTRS({}, rotation, {1, 1, 1}), {1, 0, 0});
        Check(std::abs(point.x) < .0001f && std::abs(point.z + 1) < .0001f,
              "Quaternion/column-major convention failed");
    }

    void VerifyTransitions()
    {
        auto asset = std::make_shared<ModelAsset>();
        asset->nodes.resize(1);
        asset->hierarchyOrder = {0};
        ModelChannel channel;
        channel.node = 0;
        channel.path = ModelChannelPath::Translation;
        channel.interpolation = ModelInterpolation::Linear;
        channel.times = {0, 1};
        channel.values = {{0, 0, 0, 0}, {10, 0, 0, 0}};
        asset->clips.push_back({"Walk", 1, {channel}});
        channel.times = {0, 2};
        channel.values[1].x = 20;
        asset->clips.push_back({"Run", 2, {channel}});
        channel.values[1].x = 0;
        asset->clips.push_back({"Idle", 2, {channel}});
        ModelInstance pose(asset);
        pose.Play("Walk");
        pose.Seek(.4f);
        const float before = pose.WorldMatrices()[0].values[12];
        Check(pose.TransitionTo("Run", .2f, true), "Cannot blend locomotion clips");
        Check(std::abs(pose.Time() - .8f) < .0001f &&
                  std::abs(pose.WorldMatrices()[0].values[12] - before) < .0001f,
              "Clip transition reset phase or snapped its initial pose");
        pose.Update(.1f);
        Check(std::abs(pose.WorldMatrices()[0].values[12] - 6.5f) < .0001f,
              "TRS crossfade did not interpolate its source and destination");
        const float interrupted = pose.WorldMatrices()[0].values[12];
        Check(pose.TransitionTo("Idle", .2f) &&
                  std::abs(pose.WorldMatrices()[0].values[12] - interrupted) < .0001f,
              "Interrupted crossfade did not preserve the displayed pose");
        ModelInstance copy(pose);
        copy.Update(.2f);
        Check(std::abs(copy.WorldMatrices()[0].values[12]) < .0001f &&
                  std::abs(pose.WorldMatrices()[0].values[12] - interrupted) < .0001f,
              "Crossfade completion/copy independence failed");
        const int clip = pose.ClipIndex();
        Check(!pose.TransitionTo("missing") && pose.ClipIndex() == clip,
              "Missing transition changed the active animation");
    }
} // namespace

// User-invoked --verify only. Never builds or runs automatically during development.
void VerifyLowPolyModels(Renderer& renderer)
{
    VerifySampling();
    VerifyTransitions();
    for (const char* version : {"2.0.0", "2.1.0", "2.1.7", "2.10.0"})
    {
        Check(ModelLibrary::SupportsSchemaVersion(version, 2),
              "Backward-compatible rig metadata version was rejected");
    }
    for (const char* version : {"1.0.0", "3.0.0", "2", "2.1", "2.1.0.0", "2.x.0", "2.1.0x",
                                "2.-1.0", "2.1.999999999999999999999"})
    {
        Check(!ModelLibrary::SupportsSchemaVersion(version, 2),
              "Malformed or incompatible metadata version was accepted");
    }
    ModelLibrary library("Resources/Art/LowPoly");
    std::set<std::string> mobs;
    for (const char* key : {"Enemy.Drone", "Enemy.Sentry", "Enemy.Boss"})
    {
        const auto* binding = library.FindBinding(key);
        Check(binding && binding->fitFootprint, "Missing enemy visual profile");
        for (size_t i = 0; i < binding->assets.size(); ++i)
        {
            const auto id = library.Binding(key, i);
            Check(mobs.insert(id).second, "Mob assigned to more than one combat role");
            const auto assembly = library.Single(id);
            const auto& asset = *assembly.parts.front();
            Check(!asset.skins.empty() && assembly.motions.count("Walk") &&
                      assembly.motions.count("Run"),
                  "Completed mob is not using its rigged animations");
            const float scale = binding->ScaleFor(asset, 1.1f, 1.1f);
            Check(scale > 0 &&
                      (std::max)(std::abs(asset.minimum.x), std::abs(asset.maximum.x)) * scale <=
                          .5501f &&
                      (std::max)(std::abs(asset.minimum.z), std::abs(asset.maximum.z)) * scale <=
                          .5501f,
                  "Scaled mob extends beyond its gameplay footprint");
            Check(library.Binding(key, i + binding->assets.size()) == id,
                  "Visual variant selection did not wrap");
        }
    }
    Check(mobs.size() == 7, "Not all seven completed mobs are bound");
    Check(library.Binding("NPC.Captain") == "NPC_000_Kael" &&
              library.Binding("NPC.Story") == "NPC_001_Sera",
          "Named story characters lost their authored presets");
    for (const auto& id : library.PresetIds())
    {
        ModelActor npc(library.AssembleNpc(id));
        Check(npc.Play("Walk") && npc.Play("Run") && npc.Play("Idle"),
              "Completed NPC is missing a locomotion clip");
        for (const auto& part : npc.parts)
        {
            Check(part.pose == npc.parts.front().pose, "NPC parts do not share a single pose");
        }
    }
    const auto* lamp = library.FindBinding("Prop.Lamp");
    Check(lamp && !lamp->fitFootprint &&
              std::abs(lamp->ScaleFor(*library.Load(library.Binding("Prop.Lamp")), .12f, .12f) -
                       .8f) < .0001f,
          "Lamp was shrunk to its narrow collision box");
    ModelActor pad(library.Single(library.Binding("Effect.LandingPad")), .65f,
                   RenderLayer::GroundEffect);
    Check(pad.Layer() == RenderLayer::GroundEffect, "Landing pad covers world-layer actors");
    for (const auto& id : library.AssetIds())
    {
        auto first = library.Load(id), second = library.Load(id);
        Check(first == second && first->triangleCount > 0, "CPU model cache did not reuse data");
        ModelInstance pose(first);
        for (const auto& clip : first->clips)
        {
            Check(pose.Play(clip.name, false), "Cannot play imported GLB clip");
            pose.Seek(clip.duration * .37f);
            for (const auto& matrix : pose.WorldMatrices())
            {
                for (float value : matrix.values)
                {
                    Check(std::isfinite(value), "Non-finite animated pose");
                }
            }
        }
    }
    auto assembly = library.AssembleNpc("NPC_001_Sera");
    ModelActor moving(assembly);
    moving.Update(1 / 60.f);
    for (int i = 0; i < 80; ++i)
    {
        moving.SetPosition(moving.Position() + Vec2{.03f, 0});
        moving.Update(1 / 60.f);
    }
    auto& movingPose = *moving.parts.front().pose;
    Check(movingPose.Asset().clips[movingPose.ClipIndex()].name == "Run",
          "Locomotion speed did not select the run clip");
    const int runClip = movingPose.ClipIndex();
    moving.Seek(.2f);
    moving.Update(1 / 60.f);
    Check(movingPose.ClipIndex() == runClip && movingPose.Time() > .2f,
          "One stationary frame reset the running animation");
    moving.SetPosition(moving.Position() + Vec2{.03f, 0});
    moving.Update(1 / 60.f);
    Check(movingPose.ClipIndex() == runClip, "A waypoint-sized pause caused an idle/run toggle");
    for (int i = 0; i < 20; ++i)
    {
        moving.Update(1 / 60.f);
    }
    Check(movingPose.Asset().clips[movingPose.ClipIndex()].name == "Idle",
          "Stationary NPC never settled to idle");
    for (int i = 0; i < 30; ++i)
    {
        moving.SetPosition(moving.Position() + Vec2{1, 0});
        moving.Update(1 / 60.f);
        Check(movingPose.speed >= .65f && movingPose.speed <= 1.8001f,
              "Locomotion playback rate exceeded its safe range");
    }
    ModelActor actor(assembly);
    if (!assembly.parts.front()->skins.empty())
    {
        for (const auto& part : actor.parts)
        {
            Check(part.pose == actor.parts.front().pose,
                  "NPC parts instantiate duplicate skeletons");
        }
    }
    actor.automaticLocomotion = false;
    actor.Play("Walk");
    actor.Seek(.3f);
    ModelActor copy(actor);
    copy.Seek(.8f);
    Check(copy.parts.front().pose != actor.parts.front().pose &&
              copy.parts.front().asset == actor.parts.front().asset,
          "Actor clone shares a pose or duplicates mesh data");
    if (assembly.motions.count("Turn_L_90"))
    {
        copy.PlayAction("Turn_L_90", true);
        copy.Update(20);
        Check(std::abs(copy.yaw - 1.5707963268f) < .001f, "Turn root heading was not retained");
    }
    Canvas canvas;
    ModelRenderer models;
    renderer.BeginSceneCapture();
    canvas.SetView(renderer.View());
    models.SetView(renderer.View());
    glDisable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    std::vector<unsigned char> original(renderer.SceneWidth() * renderer.SceneHeight() * 4);
    auto batched = original;
    int before = RenderStats::DrawCallCount();
    models.Draw(actor, {650, 600}, 2.f, canvas, 1, false);
    const int originalCalls = RenderStats::DrawCallCount() - before;
    glReadPixels(0, 0, renderer.SceneWidth(), renderer.SceneHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 original.data());
    glClear(GL_COLOR_BUFFER_BIT);
    before = RenderStats::DrawCallCount();
    models.Draw(actor, {650, 600}, 2.f, canvas);
    Check(RenderStats::DrawCallCount() - before < originalCalls,
          "Shared-skeleton NPC batching did not reduce actual glDraw calls");
    glReadPixels(0, 0, renderer.SceneWidth(), renderer.SceneHeight(), GL_RGBA, GL_UNSIGNED_BYTE,
                 batched.data());
    size_t covered = 0, changed = 0;
    for (size_t i = 0; i < original.size(); i += 4)
    {
        covered += original[i] || original[i + 1] || original[i + 2];
        changed += std::abs(int(original[i]) - batched[i]) > 2 ||
                   std::abs(int(original[i + 1]) - batched[i + 1]) > 2 ||
                   std::abs(int(original[i + 2]) - batched[i + 2]) > 2;
    }
    Check(covered > 100 && changed <= 2 + covered / 200,
          "NPC color batching changed appearance or depth ordering");
    size_t cache = models.CachedAssetCount();
    models.Draw(copy, {850, 600}, 2.f, canvas);
    Check(cache > 0 && cache == models.CachedAssetCount(), "GPU model data was not reused");
    Check(RenderStats::DrawCallCount() > before && glGetError() == GL_NO_ERROR,
          "Model shader / skin buffer / indexed draw verification failed");
    // Transparent actors must keep the ordered path, including palette alpha overrides.
    before = RenderStats::DrawCallCount();
    models.Draw(actor, {650, 600}, 2.f, canvas, .5f);
    Check(RenderStats::DrawCallCount() - before == originalCalls,
          "Transparent NPC was reordered by opaque batching");
    renderer.EndSceneCaptureAndComposite();
}
